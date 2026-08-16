#include "pomodoro_platform.h"

#include <inttypes.h>
#include <string.h>

#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_timer.h"
#include "pomodoro_application.h"
#include "pomodoro_board.h"
#include "pomodoro_device_view.h"

typedef struct {
    pomodoro_journal_t journal;
    pomodoro_application_t *application;
    uint64_t last_epoch_seconds;
    uint8_t countdown_log_frames;
    esp_timer_handle_t controls_timer;
    bool volume_up_candidate;
    bool volume_up_stable;
    bool volume_down_candidate;
    bool volume_down_stable;
    uint8_t volume_up_debounce;
    uint8_t volume_down_debounce;
    uint8_t power_poll_divider;
} runtime_context_t;

static const char *TAG = "pomodoro_runtime";
static runtime_context_t runtime;

static bool read_time(void *context, pomodoro_application_time_t *time)
{
    runtime_context_t *target = context;
    bool read = target != NULL && pomodoro_board_read_time(time);
    if (read) {
        target->last_epoch_seconds = time->epoch_seconds;
    }
    return read;
}

static bool resolve_local_date(void *context, uint64_t epoch_seconds,
                               int8_t utc_offset_quarter_hours,
                               pomodoro_date_t *date)
{
    return context != NULL &&
           pomodoro_board_resolve_local_date(
               epoch_seconds, utc_offset_quarter_hours, date);
}

static bool write_utc_time(void *context, uint64_t utc_epoch_seconds)
{
    return context != NULL && pomodoro_board_write_utc_time(utc_epoch_seconds);
}

static bool set_display_brightness_percent(void *context, uint8_t percent)
{
    return context != NULL && pomodoro_board_set_display_brightness(percent);
}

static bool read_device_status(
    void *context, pomodoro_application_device_status_t *status)
{
    return context != NULL && pomodoro_board_read_device_status(status);
}

static bool set_display_awake(void *context, bool awake)
{
    return context != NULL && pomodoro_board_set_display_awake(awake);
}

static bool power_off(void *context)
{
    return context != NULL && pomodoro_board_power_off();
}

static pomodoro_application_load_result_t load_state(
    void *context, pomodoro_persisted_state_t *state)
{
    runtime_context_t *target = context;
    if (target == NULL || state == NULL) {
        return POMODORO_APPLICATION_LOAD_ERROR;
    }
    pomodoro_journal_read_result_t result =
        pomodoro_journal_load(&target->journal, state);
    if (result == POMODORO_JOURNAL_READ_LOADED) {
        return POMODORO_APPLICATION_LOAD_LOADED;
    }
    return result == POMODORO_JOURNAL_READ_EMPTY
        ? POMODORO_APPLICATION_LOAD_EMPTY
        : POMODORO_APPLICATION_LOAD_ERROR;
}

static bool save_state(void *context, const pomodoro_persisted_state_t *state)
{
    runtime_context_t *target = context;
    bool saved = target != NULL && pomodoro_journal_save(&target->journal, state);
    if (!saved) {
        ESP_LOGE(TAG, "NVS state save failed");
    }
    return saved;
}

static bool render_view(void *context, const pomodoro_application_view_t *view)
{
    runtime_context_t *target = context;
    if (target != NULL && view != NULL &&
        view->timer_state == POMODORO_TIMER_RUNNING &&
        target->countdown_log_frames < 5U) {
        ESP_LOGI(TAG, "Countdown frame epoch=%" PRIu64 ", remaining=%" PRIu32,
                 target->last_epoch_seconds, view->remaining_seconds);
        ++target->countdown_log_frames;
    }
    return pomodoro_device_view_render(view);
}

static bool play_completion(void *context, uint8_t volume)
{
    return context != NULL &&
           pomodoro_board_play_completion_tone(true, volume);
}

static bool play_break_completion(void *context, uint8_t volume)
{
    return context != NULL &&
           pomodoro_board_play_completion_tone(false, volume);
}

static void dispatch_action(pomodoro_application_action_t action)
{
    bool consumed = false;
    pomodoro_application_result_t result = POMODORO_APPLICATION_OK;
    if (action != POMODORO_APPLICATION_ACTION_TICK) {
        result = pomodoro_application_handle_touch(runtime.application,
                                                    &consumed);
        if (result != POMODORO_APPLICATION_OK) {
            ESP_LOGE(TAG, "Touch wake failed: %d", result);
            return;
        }
        if (consumed) {
            return;
        }
    }
    result =
        pomodoro_application_dispatch(runtime.application, action);
    if (result != POMODORO_APPLICATION_OK) {
        ESP_LOGE(TAG, "Application action %d failed: %d", action, result);
    } else if (action == POMODORO_APPLICATION_ACTION_START) {
        ESP_LOGI(TAG, "Start touch accepted");
    }
}

static void change_brightness(uint8_t percent, bool confirmed)
{
    bool consumed = false;
    pomodoro_application_result_t result =
        pomodoro_application_handle_touch(runtime.application, &consumed);
    if (result == POMODORO_APPLICATION_OK && !consumed) {
        result = confirmed
            ? pomodoro_application_confirm_brightness(runtime.application,
                                                       percent)
            : pomodoro_application_preview_brightness(runtime.application,
                                                       percent);
    }
    if (result != POMODORO_APPLICATION_OK) {
        ESP_LOGE(TAG, "Brightness change failed: %d", result);
    }
}

static void dispatch_hardware_action(pomodoro_application_action_t action)
{
    if (!lvgl_port_lock(50)) {
        ESP_LOGW(TAG, "Hardware action lock timed out");
        return;
    }
    pomodoro_application_result_t result =
        pomodoro_application_dispatch(runtime.application, action);
    lvgl_port_unlock();
    if (result != POMODORO_APPLICATION_OK) {
        ESP_LOGE(TAG, "Hardware action %d failed: %d", action, result);
    }
}

static void debounce_volume_button(bool sample, bool *candidate,
                                   bool *stable, uint8_t *count,
                                   pomodoro_application_action_t action)
{
    if (sample != *candidate) {
        *candidate = sample;
        *count = 1U;
        return;
    }
    if (*count < 2U) {
        ++*count;
    }
    if (*count == 2U && *stable != *candidate) {
        *stable = *candidate;
        if (*stable) {
            dispatch_hardware_action(action);
        }
    }
}

static void poll_controls(void *context)
{
    runtime_context_t *target = context;
    bool increase;
    bool decrease;
    if (target == NULL ||
        !pomodoro_board_read_volume_buttons(&increase, &decrease)) {
        return;
    }
    debounce_volume_button(increase, &target->volume_up_candidate,
                           &target->volume_up_stable,
                           &target->volume_up_debounce,
                           POMODORO_APPLICATION_ACTION_VOLUME_UP);
    debounce_volume_button(decrease, &target->volume_down_candidate,
                           &target->volume_down_stable,
                           &target->volume_down_debounce,
                           POMODORO_APPLICATION_ACTION_VOLUME_DOWN);
    if (++target->power_poll_divider < 4U) {
        return;
    }
    target->power_poll_divider = 0U;
    bool short_press;
    bool long_press;
    if (pomodoro_board_read_power_events(&short_press, &long_press)) {
        if (long_press) {
            dispatch_hardware_action(POMODORO_APPLICATION_ACTION_POWER_OFF);
        } else if (short_press) {
            dispatch_hardware_action(
                POMODORO_APPLICATION_ACTION_TOGGLE_DISPLAY);
        }
    }
}

static esp_err_t fail_startup(const char *message, esp_err_t error)
{
    pomodoro_device_view_show_startup_error(message, error);
    return error;
}

esp_err_t pomodoro_platform_start(void)
{
    memset(&runtime, 0, sizeof(runtime));
    esp_err_t result = pomodoro_board_initialize_i2c();
    if (result != ESP_OK) {
        return fail_startup("I2C/PMIC initialization failed", result);
    }
    result = pomodoro_board_initialize_display();
    if (result != ESP_OK) {
        return fail_startup("Display initialization failed", result);
    }
    result = pomodoro_device_view_create(dispatch_action, change_brightness);
    if (result != ESP_OK) {
        return fail_startup("Main view initialization failed", result);
    }
    result = pomodoro_board_initialize_touch();
    if (result != ESP_OK) {
        return fail_startup("Touch initialization failed", result);
    }
    result = pomodoro_board_initialize_rtc();
    if (result != ESP_OK) {
        return fail_startup("RTC initialization failed", result);
    }
    result = pomodoro_board_initialize_controls();
    if (result != ESP_OK) {
        return fail_startup("Button initialization failed", result);
    }
    result = pomodoro_board_initialize_audio();
    if (result != ESP_OK) {
        return fail_startup("Audio initialization failed", result);
    }
    pomodoro_application_time_t boot_time;
    if (!pomodoro_board_read_time(&boot_time)) {
        return fail_startup("RTC unavailable or invalid",
                            ESP_ERR_INVALID_STATE);
    }
    if (!pomodoro_nvs_journal_init(&runtime.journal)) {
        return fail_startup("NVS initialization failed", ESP_FAIL);
    }

    pomodoro_application_platform_t platform = {
        .context = &runtime,
        .read_time = read_time,
        .resolve_local_date = resolve_local_date,
        .write_utc_time = write_utc_time,
        .set_display_brightness_percent = set_display_brightness_percent,
        .read_device_status = read_device_status,
        .set_display_awake = set_display_awake,
        .power_off = power_off,
        .load = load_state,
        .save = save_state,
        .render = render_view,
        .play_focus_complete = play_completion,
        .play_break_complete = play_break_completion,
    };
    pomodoro_application_result_t app_result =
        pomodoro_application_create(&runtime.application, &platform);
    if (app_result != POMODORO_APPLICATION_OK) {
        return fail_startup("Application initialization failed", ESP_ERR_NO_MEM);
    }
    if (!lvgl_port_lock(1000)) {
        return fail_startup("Application boot lock timed out", ESP_ERR_TIMEOUT);
    }
    app_result = pomodoro_application_dispatch(
        runtime.application, POMODORO_APPLICATION_ACTION_BOOT);
    lvgl_port_unlock();
    if (app_result != POMODORO_APPLICATION_OK) {
        return fail_startup("Application boot failed", ESP_FAIL);
    }
    result = pomodoro_device_view_start_ticks();
    if (result != ESP_OK) {
        return fail_startup("Countdown tick initialization failed", result);
    }
    const esp_timer_create_args_t controls_timer_config = {
        .callback = poll_controls,
        .arg = &runtime,
        .name = "pomodoro_controls",
    };
    result = esp_timer_create(&controls_timer_config,
                              &runtime.controls_timer);
    if (result == ESP_OK) {
        result = esp_timer_start_periodic(runtime.controls_timer, 50000U);
    }
    if (result != ESP_OK) {
        return fail_startup("Button polling failed", result);
    }

    ESP_LOGI(TAG, "Focus Ready; RTC epoch=%" PRIu64,
             boot_time.epoch_seconds);
    return ESP_OK;
}
