#include "pomodoro_focus_view.h"

#include <inttypes.h>

#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"

typedef struct {
    pomodoro_focus_action_handler_t action_handler;
    lv_obj_t *ring;
    lv_obj_t *time;
    lv_obj_t *status;
    lv_obj_t *start_button;
    lv_timer_t *tick_timer;
} focus_view_context_t;

static const char *TAG = "pomodoro_view";
static focus_view_context_t view_context;

static const char *application_error_text(pomodoro_application_error_t error)
{
    switch (error) {
    case POMODORO_APPLICATION_ERROR_CLOCK:
        return "RTC unavailable or invalid";
    case POMODORO_APPLICATION_ERROR_STORAGE_LOAD:
        return "Saved state could not be loaded";
    case POMODORO_APPLICATION_ERROR_STORAGE_SAVE:
        return "Saved state could not be written";
    case POMODORO_APPLICATION_ERROR_DISPLAY:
        return "Display update failed";
    case POMODORO_APPLICATION_ERROR_AUDIO:
        return "Audio output failed";
    default:
        return "Device error";
    }
}

static void start_clicked(lv_event_t *event)
{
    (void) event;
    if (view_context.action_handler != NULL) {
        view_context.action_handler(POMODORO_APPLICATION_ACTION_START);
    }
}

static void tick(lv_timer_t *timer)
{
    (void) timer;
    if (view_context.action_handler != NULL) {
        view_context.action_handler(POMODORO_APPLICATION_ACTION_TICK);
    }
}

esp_err_t pomodoro_focus_view_create(
    pomodoro_focus_action_handler_t action_handler)
{
    if (action_handler == NULL || !lvgl_port_lock(1000)) {
        return action_handler == NULL ? ESP_ERR_INVALID_ARG : ESP_ERR_TIMEOUT;
    }
    view_context.action_handler = action_handler;
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x07090D), 0);
    lv_obj_set_style_text_color(screen, lv_color_hex(0xF5F7FA), 0);

    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "Focus");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 38);

    view_context.ring = lv_arc_create(screen);
    lv_obj_set_size(view_context.ring, 300, 300);
    lv_obj_align(view_context.ring, LV_ALIGN_CENTER, 0, -12);
    lv_arc_set_rotation(view_context.ring, 270);
    lv_arc_set_bg_angles(view_context.ring, 0, 360);
    lv_arc_set_range(view_context.ring, 0, 1000);
    lv_arc_set_value(view_context.ring, 1000);
    lv_obj_remove_style(view_context.ring, NULL, LV_PART_KNOB);
    lv_obj_remove_flag(view_context.ring, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(view_context.ring, 13, LV_PART_MAIN);
    lv_obj_set_style_arc_color(view_context.ring, lv_color_hex(0x252A33),
                               LV_PART_MAIN);
    lv_obj_set_style_arc_width(view_context.ring, 13, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(view_context.ring, lv_color_hex(0xFF5B56),
                               LV_PART_INDICATOR);

    view_context.time = lv_label_create(screen);
    lv_label_set_text(view_context.time, "25:00");
    lv_obj_set_style_text_font(view_context.time, &lv_font_montserrat_48, 0);
    lv_obj_align(view_context.time, LV_ALIGN_CENTER, 0, -28);

    view_context.status = lv_label_create(screen);
    lv_label_set_text(view_context.status, "Starting...");
    lv_obj_set_style_text_color(view_context.status, lv_color_hex(0xAAB1BE), 0);
    lv_obj_align(view_context.status, LV_ALIGN_CENTER, 0, 30);

    view_context.start_button = lv_button_create(screen);
    lv_obj_set_size(view_context.start_button, 160, 56);
    lv_obj_align(view_context.start_button, LV_ALIGN_BOTTOM_MID, 0, -28);
    lv_obj_set_style_radius(view_context.start_button, 28, 0);
    lv_obj_set_style_bg_color(view_context.start_button,
                              lv_color_hex(0xFF5B56), 0);
    lv_obj_add_event_cb(view_context.start_button, start_clicked,
                        LV_EVENT_CLICKED, NULL);
    lv_obj_t *start_label = lv_label_create(view_context.start_button);
    lv_label_set_text(start_label, "Start");
    lv_obj_set_style_text_font(start_label, &lv_font_montserrat_24, 0);
    lv_obj_center(start_label);
    lvgl_port_unlock();
    return ESP_OK;
}

bool pomodoro_focus_view_render(const pomodoro_application_view_t *view)
{
    if (view == NULL || view_context.time == NULL) {
        return false;
    }
    if (view->phase == POMODORO_APPLICATION_PHASE_PLATFORM_ERROR) {
        lv_label_set_text(view_context.status, application_error_text(view->error));
        lv_obj_add_flag(view_context.start_button, LV_OBJ_FLAG_HIDDEN);
        lv_arc_set_value(view_context.ring, 0);
        lv_label_set_text(view_context.time, "--:--");
        ESP_LOGE(TAG, "Application platform error: %d", view->error);
        return true;
    }

    uint32_t minutes = view->remaining_seconds / 60U;
    uint32_t seconds = view->remaining_seconds % 60U;
    lv_label_set_text_fmt(view_context.time, "%02" PRIu32 ":%02" PRIu32,
                          minutes, seconds);
    uint32_t duration = (uint32_t) view->settings.focus_minutes * 60U;
    int32_t progress = duration == 0U
        ? 0
        : (int32_t) ((uint64_t) view->remaining_seconds * 1000U / duration);
    lv_arc_set_value(view_context.ring, progress);

    if (view->timer_state == POMODORO_TIMER_READY) {
        lv_label_set_text(view_context.status, "Ready to focus");
        lv_obj_remove_flag(view_context.start_button, LV_OBJ_FLAG_HIDDEN);
    } else if (view->timer_state == POMODORO_TIMER_RUNNING) {
        lv_label_set_text(view_context.status, "Focusing");
        lv_obj_add_flag(view_context.start_button, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_label_set_text(view_context.status, "Focus complete");
        lv_obj_add_flag(view_context.start_button, LV_OBJ_FLAG_HIDDEN);
    }
    return true;
}

esp_err_t pomodoro_focus_view_start_ticks(void)
{
    if (!lvgl_port_lock(1000)) {
        return ESP_ERR_TIMEOUT;
    }
    view_context.tick_timer = lv_timer_create(tick, 1000, NULL);
    lvgl_port_unlock();
    return view_context.tick_timer != NULL ? ESP_OK : ESP_ERR_NO_MEM;
}

void pomodoro_focus_view_show_startup_error(const char *message,
                                            esp_err_t error)
{
    ESP_LOGE(TAG, "%s: %s", message, esp_err_to_name(error));
    if (view_context.status != NULL && lvgl_port_lock(1000)) {
        lv_label_set_text(view_context.status, message);
        lv_label_set_text(view_context.time, "--:--");
        lv_obj_add_flag(view_context.start_button, LV_OBJ_FLAG_HIDDEN);
        lv_arc_set_value(view_context.ring, 0);
        lvgl_port_unlock();
    }
}
