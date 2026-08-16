#include "pomodoro_platform.h"

#include <inttypes.h>
#include <string.h>

#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "pomodoro_application.h"
#include "pomodoro_board.h"
#include "pomodoro_focus_view.h"

typedef struct {
    pomodoro_journal_t journal;
    pomodoro_application_t *application;
    uint64_t last_epoch_seconds;
    uint8_t countdown_log_frames;
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

static pomodoro_application_load_result_t load_state(
    void *context, pomodoro_persisted_state_t *state)
{
    runtime_context_t *target = context;
    if (target == NULL || state == NULL) {
        return POMODORO_APPLICATION_LOAD_ERROR;
    }
    return pomodoro_journal_load(&target->journal, state)
        ? POMODORO_APPLICATION_LOAD_LOADED
        : POMODORO_APPLICATION_LOAD_EMPTY;
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
    return pomodoro_focus_view_render(view);
}

static bool play_completion(void *context, uint8_t volume)
{
    (void) context;
    (void) volume;
    return true;
}

static void dispatch_action(pomodoro_application_action_t action)
{
    pomodoro_application_result_t result =
        pomodoro_application_dispatch(runtime.application, action);
    if (result != POMODORO_APPLICATION_OK) {
        ESP_LOGE(TAG, "Application action %d failed: %d", action, result);
    } else if (action == POMODORO_APPLICATION_ACTION_START) {
        ESP_LOGI(TAG, "Start touch accepted");
    }
}

static esp_err_t fail_startup(const char *message, esp_err_t error)
{
    pomodoro_focus_view_show_startup_error(message, error);
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
    result = pomodoro_focus_view_create(dispatch_action);
    if (result != ESP_OK) {
        return fail_startup("Focus view initialization failed", result);
    }
    result = pomodoro_board_initialize_touch();
    if (result != ESP_OK) {
        return fail_startup("Touch initialization failed", result);
    }
    result = pomodoro_board_initialize_rtc();
    if (result != ESP_OK) {
        return fail_startup("RTC initialization failed", result);
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
        .load = load_state,
        .save = save_state,
        .render = render_view,
        .play_focus_complete = play_completion,
        .play_break_complete = play_completion,
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
    result = pomodoro_focus_view_start_ticks();
    if (result != ESP_OK) {
        return fail_startup("Countdown tick initialization failed", result);
    }

    ESP_LOGI(TAG, "Focus Ready; RTC epoch=%" PRIu64,
             boot_time.epoch_seconds);
    return ESP_OK;
}
