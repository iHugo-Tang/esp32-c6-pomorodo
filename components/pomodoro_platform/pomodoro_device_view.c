#include "pomodoro_device_view.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"
#include "qrcode.h"

LV_DRAW_BUF_DEFINE_STATIC(time_sync_qr_buffer, 180, 180,
                          LV_COLOR_FORMAT_I1);

typedef struct {
    int16_t x;
    int16_t y;
    uint32_t color;
} confetti_piece_t;

static const confetti_piece_t completion_confetti[] = {
    {.x = -72, .y = 8, .color = 0xFFD166},
    {.x = -54, .y = 48, .color = 0x5B8CFF},
    {.x = -42, .y = -2, .color = 0x4DD17A},
    {.x = 44, .y = -4, .color = 0xFFD166},
    {.x = 58, .y = 45, .color = 0xFF5C5C},
    {.x = 72, .y = 14, .color = 0x5B8CFF},
};

enum {
    COMPLETION_CONFETTI_COUNT =
        sizeof(completion_confetti) / sizeof(completion_confetti[0]),
    TIMER_RING_CENTER_Y = -18,
    COMPLETION_MANUAL_GROUP_Y = 12,
    COMPLETION_MANUAL_PRIMARY_Y = 83,
    COMPLETION_AUTO_GROUP_Y = -11,
    COMPLETION_AUTO_PRIMARY_Y = 59,
    COMPLETION_AUTO_SECONDARY_Y = 112,
    SETTINGS_SECTION_TEXT_COLOR = 0xAAB1BE,
    SETTINGS_ITEM_TEXT_COLOR = 0xF5F7FA,
    SETTINGS_VALUE_TEXT_COLOR = 0x9FC2FF,
};

typedef struct {
    pomodoro_application_action_t action;
    lv_obj_t *button;
    lv_obj_t *label;
} auxiliary_button_t;

static const struct {
    pomodoro_application_action_t action;
    const char *text;
} auxiliary_button_definitions[] = {
    {POMODORO_APPLICATION_ACTION_OPEN_FOCUS_MINUTES, "Focus"},
    {POMODORO_APPLICATION_ACTION_OPEN_SHORT_BREAK_MINUTES, "Short break"},
    {POMODORO_APPLICATION_ACTION_OPEN_LONG_BREAK_MINUTES, "Long break"},
    {POMODORO_APPLICATION_ACTION_OPEN_DAILY_GOAL, "Daily goal"},
    {POMODORO_APPLICATION_ACTION_TOGGLE_AUTO_BREAK, "Auto breaks"},
    {POMODORO_APPLICATION_ACTION_TOGGLE_AUTO_FOCUS, "Auto focus"},
    {POMODORO_APPLICATION_ACTION_TOGGLE_SOUND, "Sound"},
    {POMODORO_APPLICATION_ACTION_OPEN_STATISTICS, "Stats"},
    {POMODORO_APPLICATION_ACTION_OPEN_SETTINGS, "Settings"},
    {POMODORO_APPLICATION_ACTION_OPEN_BRIGHTNESS, "Brightness"},
    {POMODORO_APPLICATION_ACTION_POWER_OFF, "Power off"},
    {POMODORO_APPLICATION_ACTION_OPEN_DATE_TIME, "Date & time"},
    {POMODORO_APPLICATION_ACTION_OPEN_TIME_SYNC, "Sync from iPhone"},
    {POMODORO_APPLICATION_ACTION_MANUAL_YEAR_DOWN, "Year -"},
    {POMODORO_APPLICATION_ACTION_MANUAL_YEAR_UP, "Year +"},
    {POMODORO_APPLICATION_ACTION_MANUAL_MONTH_DOWN, "Month -"},
    {POMODORO_APPLICATION_ACTION_MANUAL_MONTH_UP, "Month +"},
    {POMODORO_APPLICATION_ACTION_MANUAL_DAY_DOWN, "Day -"},
    {POMODORO_APPLICATION_ACTION_MANUAL_DAY_UP, "Day +"},
    {POMODORO_APPLICATION_ACTION_MANUAL_HOUR_DOWN, "Hour -"},
    {POMODORO_APPLICATION_ACTION_MANUAL_HOUR_UP, "Hour +"},
    {POMODORO_APPLICATION_ACTION_MANUAL_MINUTE_DOWN, "Minute -"},
    {POMODORO_APPLICATION_ACTION_MANUAL_MINUTE_UP, "Minute +"},
    {POMODORO_APPLICATION_ACTION_MANUAL_OFFSET_DOWN, "UTC -"},
    {POMODORO_APPLICATION_ACTION_MANUAL_OFFSET_UP, "UTC +"},
    {POMODORO_APPLICATION_ACTION_MANUAL_TIME_CONFIRM, "Set time"},
    {POMODORO_APPLICATION_ACTION_NUMERIC_DECREMENT, LV_SYMBOL_DOWN},
    {POMODORO_APPLICATION_ACTION_NUMERIC_INCREMENT, LV_SYMBOL_UP},
    {POMODORO_APPLICATION_ACTION_NUMERIC_CONFIRM, "Confirm"},
    {POMODORO_APPLICATION_ACTION_BACK, "Back"},
};

enum {
    AUXILIARY_BUTTON_COUNT = sizeof(auxiliary_button_definitions) /
        sizeof(auxiliary_button_definitions[0]),
};

typedef struct {
    pomodoro_device_action_handler_t action_handler;
    pomodoro_application_view_t last_view;
    lv_obj_t *title;
    lv_obj_t *ring;
    lv_obj_t *time;
    lv_obj_t *status;
    lv_obj_t *primary_button;
    lv_obj_t *primary_label;
    lv_obj_t *secondary_button;
    lv_obj_t *secondary_label;
    lv_obj_t *completion_group;
    lv_obj_t *tomato;
    lv_obj_t *cup;
    lv_obj_t *cup_handle;
    lv_obj_t *cup_steam[2];
    lv_obj_t *completion_title;
    lv_obj_t *completion_body;
    lv_obj_t *completion_countdown;
    lv_obj_t *set_label;
    lv_obj_t *set_markers[4];
    lv_obj_t *confetti[COMPLETION_CONFETTI_COUNT];
    lv_obj_t *mode_dots[3];
    lv_obj_t *boundary_feedback;
    lv_obj_t *confirmation_mask;
    lv_obj_t *confirmation_overlay;
    lv_obj_t *confirmation_title;
    lv_obj_t *confirmation_body;
    lv_obj_t *confirmation_button_label;
    lv_obj_t *mode_transition_overlay;
    lv_obj_t *menu_button;
    lv_obj_t *top_power_status;
    lv_obj_t *page_panel;
    lv_obj_t *page_title;
    lv_obj_t *page_back_button;
    lv_obj_t *page_body;
    lv_obj_t *page_content;
    lv_obj_t *statistics_metrics;
    lv_obj_t *statistics_focus_card;
    lv_obj_t *statistics_focus_value;
    lv_obj_t *statistics_goal_card;
    lv_obj_t *statistics_goal_value;
    lv_obj_t *statistics_goal_bar;
    lv_obj_t *settings_switches;
    lv_obj_t *settings_today_value;
    lv_obj_t *settings_timer_values[4];
    lv_obj_t *settings_volume_value;
    lv_obj_t *settings_brightness_value;
    lv_obj_t *settings_datetime_value;
    lv_obj_t *settings_time_sync_value;
    lv_obj_t *settings_power_value;
    lv_obj_t *auto_breaks_switch;
    lv_obj_t *auto_focus_switch;
    lv_obj_t *sound_switch;
    lv_obj_t *auxiliary_buttons_container;
    lv_obj_t *statistics_chart;
    lv_obj_t *numeric_roller;
    lv_obj_t *time_sync_qr;
    lv_obj_t *statistics_bars[7];
    lv_obj_t *statistics_dates[7];
    lv_obj_t *statistics_counts[7];
    auxiliary_button_t auxiliary_buttons[AUXILIARY_BUTTON_COUNT];
    lv_obj_t *status_overlay;
    lv_obj_t *rtc_warning;
    lv_obj_t *battery_warning;
    lv_obj_t *volume_feedback;
    lv_obj_t *volume_icon;
    lv_obj_t *volume_segments[5];
    lv_obj_t *volume_value;
    lv_timer_t *tick_timer;
    lv_timer_t *mode_transition_timer;
    pomodoro_application_action_t pending_mode_action;
    int32_t gesture_start_x;
    bool numeric_roller_rendering;
    char rendered_time_sync_uri[128];
} device_view_context_t;

typedef struct {
    const char *name;
    const char *action_name;
    uint32_t accent;
} mode_presentation_t;

static const char *TAG = "pomodoro_view";
static device_view_context_t view_context;
static lv_font_t countdown_font;

static bool countdown_get_glyph_dsc(const lv_font_t *font,
                                    lv_font_glyph_dsc_t *glyph,
                                    uint32_t letter, uint32_t next_letter)
{
    (void) font;
    (void) next_letter;
    bool found = lv_font_get_glyph_dsc(&lv_font_montserrat_48, glyph,
                                       letter, 0U);
    if (found && letter >= '0' && letter <= '9') {
        glyph->adv_w = 32U;
    }
    return found;
}

static void dispatch(pomodoro_application_action_t action)
{
    if (view_context.action_handler != NULL) {
        view_context.action_handler(action);
    }
}

static void action_clicked(lv_event_t *event)
{
    dispatch((pomodoro_application_action_t) (uintptr_t)
        lv_event_get_user_data(event));
}

static void numeric_roller_changed(lv_event_t *event)
{
    (void) event;
    if (view_context.numeric_roller_rendering) {
        return;
    }
    uint16_t selected = view_context.last_view.numeric_minimum +
        (uint16_t) lv_roller_get_selected(view_context.numeric_roller) *
            view_context.last_view.numeric_step;
    pomodoro_application_action_t action = selected >
            view_context.last_view.numeric_draft_value
        ? POMODORO_APPLICATION_ACTION_NUMERIC_INCREMENT
        : POMODORO_APPLICATION_ACTION_NUMERIC_DECREMENT;
    uint16_t difference = selected > view_context.last_view.numeric_draft_value
        ? selected - view_context.last_view.numeric_draft_value
        : view_context.last_view.numeric_draft_value - selected;
    uint16_t steps = difference / view_context.last_view.numeric_step;
    for (uint16_t step = 0; step < steps; step++) {
        dispatch(action);
    }
}

static void draw_time_sync_qr(esp_qrcode_handle_t qrcode, void *user_data)
{
    lv_obj_t *canvas = user_data;
    int modules = esp_qrcode_get_size(qrcode);
    const int quiet_modules = 4;
    int scale = 180 / (modules + quiet_modules * 2);
    int image_size = (modules + quiet_modules * 2) * scale;
    int origin = (180 - image_size) / 2 + quiet_modules * scale;
    lv_canvas_fill_bg(canvas, lv_color_white(), LV_OPA_COVER);
    for (int y = 0; y < modules; y++) {
        for (int x = 0; x < modules; x++) {
            if (!esp_qrcode_get_module(qrcode, x, y)) {
                continue;
            }
            for (int pixel_y = 0; pixel_y < scale; pixel_y++) {
                for (int pixel_x = 0; pixel_x < scale; pixel_x++) {
                    lv_canvas_set_px(canvas, origin + x * scale + pixel_x,
                                     origin + y * scale + pixel_y,
                                     lv_color_black(), LV_OPA_COVER);
                }
            }
        }
    }
}

static void render_time_sync_qr(const char *join_uri)
{
    if (join_uri == NULL || join_uri[0] == '\0' ||
        strcmp(join_uri, view_context.rendered_time_sync_uri) == 0) {
        return;
    }
    esp_qrcode_config_t config = ESP_QRCODE_CONFIG_DEFAULT();
    config.display_func_with_cb = draw_time_sync_qr;
    config.user_data = view_context.time_sync_qr;
    config.max_qrcode_version = 8;
    config.qrcode_ecc_level = ESP_QRCODE_ECC_MED;
    if (esp_qrcode_generate(&config, join_uri) == ESP_OK) {
        strlcpy(view_context.rendered_time_sync_uri, join_uri,
                sizeof(view_context.rendered_time_sync_uri));
    }
}

static void clear_time_sync_qr(void)
{
    memset(view_context.rendered_time_sync_uri, 0,
           sizeof(view_context.rendered_time_sync_uri));
    lv_canvas_fill_bg(view_context.time_sync_qr, lv_color_white(),
                      LV_OPA_COVER);
}

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

static mode_presentation_t mode_presentation(pomodoro_mode_t mode)
{
    switch (mode) {
    case POMODORO_MODE_SHORT_BREAK:
        return (mode_presentation_t) {
            .name = "Short break",
            .action_name = "short break",
            .accent = 0x4DD17A,
        };
    case POMODORO_MODE_LONG_BREAK:
        return (mode_presentation_t) {
            .name = "Long break",
            .action_name = "long break",
            .accent = 0x5B8CFF,
        };
    default:
        return (mode_presentation_t) {
            .name = "Focus",
            .action_name = "focus",
            .accent = 0xFF5C5C,
        };
    }
}

static void primary_clicked(lv_event_t *event)
{
    (void) event;
    const pomodoro_application_view_t *view = &view_context.last_view;
    if (view->timer_state == POMODORO_TIMER_COMPLETION) {
        dispatch(POMODORO_APPLICATION_ACTION_START_NEXT);
    } else if (view->timer_state == POMODORO_TIMER_RUNNING) {
        dispatch(POMODORO_APPLICATION_ACTION_PAUSE);
    } else if (view->timer_state == POMODORO_TIMER_PAUSED) {
        dispatch(POMODORO_APPLICATION_ACTION_RESUME);
    } else {
        dispatch(POMODORO_APPLICATION_ACTION_START);
    }
}

static void secondary_clicked(lv_event_t *event)
{
    (void) event;
    const pomodoro_application_view_t *view = &view_context.last_view;
    if (view->timer_state == POMODORO_TIMER_COMPLETION) {
        dispatch(view->auto_start_pending
            ? POMODORO_APPLICATION_ACTION_CANCEL_AUTO_START
            : POMODORO_APPLICATION_ACTION_NOT_NOW);
    } else {
        dispatch(POMODORO_APPLICATION_ACTION_REQUEST_RESET);
    }
}

static void confirmation_cancelled(lv_event_t *event)
{
    (void) event;
    dispatch(POMODORO_APPLICATION_ACTION_CANCEL_CONFIRMATION);
}

static void confirmation_accepted(lv_event_t *event)
{
    (void) event;
    dispatch(view_context.last_view.confirmation ==
                     POMODORO_APPLICATION_CONFIRM_RESET
                 ? POMODORO_APPLICATION_ACTION_CONFIRM_RESET
                 : POMODORO_APPLICATION_ACTION_CONFIRM_MODE_SWITCH);
}

static void mode_transition_cleanup(lv_timer_t *timer);

static void mode_transition_finished(lv_timer_t *timer)
{
    (void) timer;
    pomodoro_application_action_t action = view_context.pending_mode_action;
    dispatch(action);
    lv_obj_move_foreground(view_context.mode_transition_overlay);
    lv_obj_fade_out(view_context.mode_transition_overlay, 60, 0);
    view_context.mode_transition_timer = lv_timer_create(
        mode_transition_cleanup, 60, NULL);
    if (view_context.mode_transition_timer != NULL) {
        lv_timer_set_repeat_count(view_context.mode_transition_timer, 1);
    } else {
        lv_obj_add_flag(view_context.mode_transition_overlay,
                        LV_OBJ_FLAG_HIDDEN);
    }
}

static void mode_transition_cleanup(lv_timer_t *timer)
{
    (void) timer;
    view_context.mode_transition_timer = NULL;
    lv_obj_add_flag(view_context.mode_transition_overlay, LV_OBJ_FLAG_HIDDEN);
}

static void gesture(lv_event_t *event)
{
    lv_indev_t *indev = lv_indev_active();
    if (indev == NULL) {
        return;
    }
    lv_point_t point;
    lv_indev_get_point(indev, &point);
    if (lv_event_get_code(event) == LV_EVENT_PRESSED) {
        view_context.gesture_start_x = point.x;
        return;
    }
    if (view_context.last_view.timer_state == POMODORO_TIMER_COMPLETION ||
        view_context.last_view.confirmation !=
            POMODORO_APPLICATION_CONFIRM_NONE ||
        view_context.mode_transition_timer != NULL) {
        return;
    }
    lv_dir_t direction = lv_indev_get_gesture_dir(indev);
    if (view_context.last_view.page != POMODORO_APPLICATION_PAGE_MAIN) {
        if (view_context.last_view.page !=
                POMODORO_APPLICATION_PAGE_SETTINGS &&
            direction == LV_DIR_RIGHT && view_context.gesture_start_x < 32 &&
            point.x - view_context.gesture_start_x >= 72) {
            dispatch(POMODORO_APPLICATION_ACTION_BACK);
            lv_event_stop_bubbling(event);
        }
        return;
    }
    if (view_context.gesture_start_x < 32) {
        return;
    }
    if (direction == LV_DIR_LEFT) {
        view_context.pending_mode_action = POMODORO_APPLICATION_ACTION_MODE_NEXT;
        if (view_context.last_view.mode == POMODORO_MODE_LONG_BREAK) {
            dispatch(view_context.pending_mode_action);
            return;
        }
    } else if (direction == LV_DIR_RIGHT) {
        view_context.pending_mode_action =
            POMODORO_APPLICATION_ACTION_MODE_PREVIOUS;
        if (view_context.last_view.mode == POMODORO_MODE_FOCUS) {
            dispatch(view_context.pending_mode_action);
            return;
        }
    } else {
        return;
    }
    lv_obj_set_style_opa(view_context.mode_transition_overlay,
                         LV_OPA_TRANSP, 0);
    lv_obj_remove_flag(view_context.mode_transition_overlay,
                       LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(view_context.mode_transition_overlay);
    lv_obj_fade_in(view_context.mode_transition_overlay, 60, 0);
    view_context.mode_transition_timer = lv_timer_create(
        mode_transition_finished, 60, NULL);
    if (view_context.mode_transition_timer != NULL) {
        lv_timer_set_repeat_count(view_context.mode_transition_timer, 1);
    } else {
        dispatch(view_context.pending_mode_action);
        lv_obj_add_flag(view_context.mode_transition_overlay,
                        LV_OBJ_FLAG_HIDDEN);
    }
}

static void tick(lv_timer_t *timer)
{
    (void) timer;
    dispatch(POMODORO_APPLICATION_ACTION_TICK);
}

static lv_obj_t *make_text_button(lv_obj_t *parent, lv_event_cb_t callback,
                                  const char *text)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_set_size(button, 150, 48);
    lv_obj_set_style_radius(button, 24, 0);
    lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, NULL);
    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    return button;
}

static lv_obj_t *make_action_button(lv_obj_t *parent,
                                    pomodoro_application_action_t action,
                                    const char *text)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_set_size(button, 150, 48);
    lv_obj_set_style_radius(button, 24, 0);
    lv_obj_add_event_cb(button, action_clicked, LV_EVENT_CLICKED,
                        (void *) (uintptr_t) action);
    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    return button;
}

static lv_obj_t *make_setting_switch_row(
    lv_obj_t *parent, const char *text,
    pomodoro_application_action_t action, lv_obj_t **toggle)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, LV_PCT(100), 58);
    lv_obj_set_style_bg_color(row, lv_color_hex(0x1A1F27), 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_radius(row, 12, 0);
    lv_obj_add_event_cb(row, action_clicked, LV_EVENT_CLICKED,
                        (void *) (uintptr_t) action);
    lv_obj_t *label = lv_label_create(row);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(
        label, lv_color_hex(SETTINGS_ITEM_TEXT_COLOR), 0);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 4, 0);
    *toggle = lv_switch_create(row);
    lv_obj_align(*toggle, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_obj_add_event_cb(*toggle, action_clicked, LV_EVENT_CLICKED,
                        (void *) (uintptr_t) action);
    return row;
}

static lv_obj_t *make_setting_action_row(
    lv_obj_t *parent, const char *text,
    pomodoro_application_action_t action, lv_obj_t **value_label)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, LV_PCT(100), 56);
    lv_obj_set_style_bg_color(row, lv_color_hex(0x1A1F27), 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_radius(row, 12, 0);
    lv_obj_add_event_cb(row, action_clicked, LV_EVENT_CLICKED,
                        (void *) (uintptr_t) action);
    lv_obj_t *label = lv_label_create(row);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(
        label, lv_color_hex(SETTINGS_ITEM_TEXT_COLOR), 0);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 4, 0);
    *value_label = lv_label_create(row);
    lv_obj_set_style_text_color(
        *value_label, lv_color_hex(SETTINGS_VALUE_TEXT_COLOR), 0);
    lv_obj_align(*value_label, LV_ALIGN_RIGHT_MID, -4, 0);
    return row;
}

static lv_obj_t *make_settings_section_label(lv_obj_t *parent,
                                             const char *text,
                                             bool separated)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(
        label, lv_color_hex(SETTINGS_SECTION_TEXT_COLOR), 0);
    if (separated) {
        lv_obj_set_style_margin_top(label, 14, 0);
    }
    return label;
}

static void create_completion_visuals(lv_obj_t *parent)
{
    view_context.tomato = lv_obj_create(parent);
    lv_obj_remove_style_all(view_context.tomato);
    lv_obj_set_size(view_context.tomato, 74, 64);
    lv_obj_set_style_bg_opa(view_context.tomato, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(view_context.tomato, lv_color_hex(0xFF5C5C), 0);
    lv_obj_set_style_radius(view_context.tomato, 32, 0);
    lv_obj_align(view_context.tomato, LV_ALIGN_TOP_MID, 0, 2);
    lv_obj_t *leaf = lv_obj_create(view_context.tomato);
    lv_obj_remove_style_all(leaf);
    lv_obj_set_size(leaf, 30, 12);
    lv_obj_set_style_bg_opa(leaf, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(leaf, lv_color_hex(0x4DD17A), 0);
    lv_obj_set_style_radius(leaf, 6, 0);
    lv_obj_align(leaf, LV_ALIGN_TOP_MID, 0, -2);
    for (uint8_t index = 0; index < 2U; index++) {
        lv_obj_t *eye = lv_obj_create(view_context.tomato);
        lv_obj_remove_style_all(eye);
        lv_obj_set_size(eye, 6, 6);
        lv_obj_set_style_bg_opa(eye, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(eye, lv_color_hex(0x542020), 0);
        lv_obj_set_style_radius(eye, LV_RADIUS_CIRCLE, 0);
        lv_obj_align(eye, LV_ALIGN_CENTER, index == 0U ? -13 : 13, -3);
    }
    lv_obj_t *smile = lv_arc_create(view_context.tomato);
    lv_obj_set_size(smile, 30, 18);
    lv_arc_set_bg_angles(smile, 20, 160);
    lv_obj_remove_style(smile, NULL, LV_PART_INDICATOR);
    lv_obj_remove_style(smile, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_width(smile, 3, LV_PART_MAIN);
    lv_obj_set_style_arc_color(smile, lv_color_hex(0x542020), LV_PART_MAIN);
    lv_obj_remove_flag(smile, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(smile, LV_ALIGN_CENTER, 6, 13);

    for (uint8_t index = 0; index < COMPLETION_CONFETTI_COUNT; index++) {
        lv_obj_t *piece = lv_obj_create(parent);
        view_context.confetti[index] = piece;
        lv_obj_remove_style_all(piece);
        lv_obj_set_size(piece, 7, 12);
        lv_obj_set_style_bg_opa(piece, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(piece,
                                  lv_color_hex(completion_confetti[index].color),
                                  0);
        lv_obj_set_style_radius(piece, 3, 0);
        lv_obj_align(piece, LV_ALIGN_TOP_MID, completion_confetti[index].x,
                     completion_confetti[index].y);
        lv_obj_add_flag(piece, LV_OBJ_FLAG_HIDDEN);
    }

    view_context.cup = lv_obj_create(parent);
    lv_obj_remove_style_all(view_context.cup);
    lv_obj_set_size(view_context.cup, 78, 58);
    lv_obj_set_style_bg_opa(view_context.cup, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(view_context.cup, lv_color_hex(0x5B8CFF), 0);
    lv_obj_set_style_radius(view_context.cup, 10, 0);
    lv_obj_align(view_context.cup, LV_ALIGN_TOP_MID, -6, 8);
    view_context.cup_handle = lv_obj_create(parent);
    lv_obj_remove_style_all(view_context.cup_handle);
    lv_obj_set_size(view_context.cup_handle, 24, 30);
    lv_obj_set_style_border_width(view_context.cup_handle, 7, 0);
    lv_obj_set_style_border_color(view_context.cup_handle,
                                  lv_color_hex(0x5B8CFF), 0);
    lv_obj_set_style_bg_opa(view_context.cup_handle, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(view_context.cup_handle, 12, 0);
    lv_obj_align(view_context.cup_handle, LV_ALIGN_TOP_MID, 43, 22);
    for (uint8_t index = 0; index < 2U; index++) {
        lv_obj_t *steam = lv_obj_create(parent);
        view_context.cup_steam[index] = steam;
        lv_obj_remove_style_all(steam);
        lv_obj_set_size(steam, 5, 18);
        lv_obj_set_style_bg_opa(steam, LV_OPA_60, 0);
        lv_obj_set_style_bg_color(steam, lv_color_hex(0xAAB1BE), 0);
        lv_obj_set_style_radius(steam, 3, 0);
        lv_obj_align(steam, LV_ALIGN_TOP_MID, index == 0U ? -18 : 12, -12);
    }
}

esp_err_t pomodoro_device_view_create(
    pomodoro_device_action_handler_t action_handler)
{
    if (action_handler == NULL || !lvgl_port_lock(1000)) {
        return action_handler == NULL
            ? ESP_ERR_INVALID_ARG
            : ESP_ERR_TIMEOUT;
    }
    memset(&view_context, 0, sizeof(view_context));
    view_context.action_handler = action_handler;
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x07090D), 0);
    lv_obj_set_style_text_color(screen, lv_color_hex(0xF5F7FA), 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(screen, gesture, LV_EVENT_GESTURE, NULL);
    lv_obj_add_event_cb(screen, gesture, LV_EVENT_PRESSED, NULL);
    for (lv_indev_t *indev = lv_indev_get_next(NULL); indev != NULL;
         indev = lv_indev_get_next(indev)) {
        if (lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER) {
            lv_indev_set_gesture_min_distance(indev, 72);
        }
    }

    view_context.title = lv_label_create(screen);
    lv_obj_set_width(view_context.title, 260);
    lv_obj_set_style_text_font(view_context.title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_align(view_context.title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(view_context.title, LV_ALIGN_TOP_MID, 0, 24);
    view_context.menu_button = make_action_button(
        screen, POMODORO_APPLICATION_ACTION_OPEN_MENU, LV_SYMBOL_LIST);
    lv_obj_set_size(view_context.menu_button, 52, 38);
    lv_obj_align(view_context.menu_button, LV_ALIGN_TOP_LEFT, 18, 18);
    view_context.top_power_status = lv_label_create(screen);
    lv_obj_set_width(view_context.top_power_status, 120);
    lv_obj_set_style_text_align(view_context.top_power_status,
                                LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(view_context.top_power_status, LV_ALIGN_TOP_RIGHT, -18, 28);

    view_context.ring = lv_arc_create(screen);
    lv_obj_set_size(view_context.ring, 288, 288);
    lv_obj_align(view_context.ring, LV_ALIGN_CENTER, 0, TIMER_RING_CENTER_Y);
    lv_arc_set_rotation(view_context.ring, 270);
    lv_arc_set_bg_angles(view_context.ring, 0, 360);
    lv_arc_set_range(view_context.ring, 0, 1000);
    lv_obj_remove_style(view_context.ring, NULL, LV_PART_KNOB);
    lv_obj_remove_flag(view_context.ring, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(view_context.ring, 13, LV_PART_MAIN);
    lv_obj_set_style_arc_color(view_context.ring, lv_color_hex(0x252A33),
                               LV_PART_MAIN);
    lv_obj_set_style_arc_width(view_context.ring, 13, LV_PART_INDICATOR);

    view_context.status = lv_label_create(screen);
    lv_obj_set_style_text_color(view_context.status, lv_color_hex(0xAAB1BE), 0);
    lv_obj_align(view_context.status, LV_ALIGN_CENTER, 0, -78);
    lv_obj_add_flag(view_context.status, LV_OBJ_FLAG_HIDDEN);
    view_context.time = lv_label_create(screen);
    countdown_font = lv_font_montserrat_48;
    countdown_font.get_glyph_dsc = countdown_get_glyph_dsc;
    countdown_font.kerning = LV_FONT_KERNING_NONE;
    lv_obj_set_style_text_font(view_context.time, &countdown_font, 0);
    lv_obj_align(view_context.time, LV_ALIGN_CENTER, 0, -25);

    view_context.primary_button = make_text_button(screen, primary_clicked,
                                                   "Start");
    view_context.primary_label = lv_obj_get_child(view_context.primary_button, 0);
    lv_obj_set_style_text_font(view_context.primary_label,
                               &lv_font_montserrat_24, 0);
    lv_obj_set_size(view_context.primary_button, 220, 56);
    lv_obj_align(view_context.primary_button, LV_ALIGN_CENTER, 0, 48);
    view_context.secondary_button = make_text_button(screen, secondary_clicked,
                                                     "Reset");
    view_context.secondary_label = lv_obj_get_child(view_context.secondary_button, 0);
    lv_obj_set_style_bg_opa(view_context.secondary_button, LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_color(view_context.secondary_button,
                                lv_color_hex(0xAAB1BE), 0);
    lv_obj_align(view_context.secondary_button, LV_ALIGN_CENTER, 0, 101);

    view_context.completion_group = lv_obj_create(screen);
    lv_obj_remove_style_all(view_context.completion_group);
    lv_obj_remove_flag(view_context.completion_group, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(view_context.completion_group, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(view_context.completion_group, 360, 250);
    lv_obj_align(view_context.completion_group, LV_ALIGN_CENTER, 0, -22);
    create_completion_visuals(view_context.completion_group);
    view_context.completion_title = lv_label_create(view_context.completion_group);
    lv_obj_set_style_text_font(view_context.completion_title,
                               &lv_font_montserrat_24, 0);
    lv_obj_align(view_context.completion_title, LV_ALIGN_TOP_MID, 0, 82);
    view_context.completion_body = lv_label_create(view_context.completion_group);
    lv_obj_set_style_text_font(view_context.completion_body,
                               &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(view_context.completion_body,
                                lv_color_hex(0xAAB1BE), 0);
    lv_obj_set_style_text_align(view_context.completion_body,
                                LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(view_context.completion_body, LV_ALIGN_TOP_MID, 0, 118);
    view_context.completion_countdown = lv_label_create(
        view_context.completion_group);
    lv_obj_align(view_context.completion_countdown, LV_ALIGN_TOP_MID, 0, 150);
    lv_obj_add_flag(view_context.completion_group, LV_OBJ_FLAG_HIDDEN);

    view_context.set_label = lv_label_create(screen);
    lv_obj_set_style_text_font(view_context.set_label,
                               &lv_font_montserrat_16, 0);
    lv_obj_align(view_context.set_label, LV_ALIGN_BOTTOM_MID, -68, -48);
    for (uint8_t index = 0; index < 4U; index++) {
        lv_obj_t *marker = lv_obj_create(screen);
        view_context.set_markers[index] = marker;
        lv_obj_remove_style_all(marker);
        lv_obj_set_size(marker, 15, 15);
        lv_obj_set_style_radius(marker, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(marker, 2, 0);
        lv_obj_align(marker, LV_ALIGN_BOTTOM_MID, index * 24 - 5, -48);
    }
    for (uint8_t index = 0; index < 3U; index++) {
        lv_obj_t *dot = lv_obj_create(screen);
        view_context.mode_dots[index] = dot;
        lv_obj_remove_style_all(dot);
        lv_obj_set_size(dot, 8, 8);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_align(dot, LV_ALIGN_BOTTOM_MID, (int32_t) index * 18 - 18, -22);
    }
    view_context.boundary_feedback = lv_label_create(screen);
    lv_label_set_text(view_context.boundary_feedback, "End of modes");
    lv_obj_set_style_text_color(view_context.boundary_feedback,
                                lv_color_hex(0xAAB1BE), 0);
    lv_obj_align(view_context.boundary_feedback, LV_ALIGN_BOTTOM_MID, 0, -76);
    lv_obj_add_flag(view_context.boundary_feedback, LV_OBJ_FLAG_HIDDEN);

    view_context.page_panel = lv_obj_create(screen);
    lv_obj_set_size(view_context.page_panel, 480, 480);
    lv_obj_align(view_context.page_panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(view_context.page_panel,
                              lv_color_hex(0x07090D), 0);
    lv_obj_set_style_bg_opa(view_context.page_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(view_context.page_panel, 0, 0);
    lv_obj_set_style_radius(view_context.page_panel, 0, 0);
    lv_obj_set_style_pad_all(view_context.page_panel, 18, 0);
    lv_obj_set_flex_flow(view_context.page_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_remove_flag(view_context.page_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(view_context.page_panel, gesture, LV_EVENT_GESTURE,
                        NULL);
    lv_obj_add_event_cb(view_context.page_panel, gesture, LV_EVENT_PRESSED,
                        NULL);
    view_context.page_title = lv_label_create(view_context.page_panel);
    lv_obj_set_width(view_context.page_title, LV_PCT(100));
    lv_obj_set_style_text_font(view_context.page_title,
                               &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_align(view_context.page_title,
                                LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(view_context.page_title,
                                lv_color_hex(0xF5F7FA), 0);
    view_context.page_back_button = make_action_button(
        view_context.page_panel, POMODORO_APPLICATION_ACTION_BACK,
        LV_SYMBOL_LEFT);
    lv_obj_set_size(view_context.page_back_button, 52, 38);
    lv_obj_add_flag(view_context.page_back_button, LV_OBJ_FLAG_FLOATING);
    lv_obj_add_flag(view_context.page_back_button, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_align(view_context.page_back_button, LV_ALIGN_TOP_LEFT, 0, -8);
    lv_obj_add_flag(view_context.page_back_button, LV_OBJ_FLAG_HIDDEN);
    view_context.page_content = lv_obj_create(view_context.page_panel);
    lv_obj_set_width(view_context.page_content, LV_PCT(100));
    lv_obj_set_height(view_context.page_content, 0);
    lv_obj_set_flex_grow(view_context.page_content, 1);
    lv_obj_set_style_bg_opa(view_context.page_content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(view_context.page_content, 0, 0);
    lv_obj_set_style_pad_all(view_context.page_content, 0, 0);
    lv_obj_set_style_pad_row(view_context.page_content, 10, 0);
    lv_obj_set_flex_flow(view_context.page_content, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(view_context.page_content, LV_OBJ_FLAG_EVENT_BUBBLE);
    view_context.page_body = lv_label_create(view_context.page_content);
    lv_obj_set_width(view_context.page_body, LV_PCT(100));
    lv_label_set_long_mode(view_context.page_body, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(view_context.page_body,
                                lv_color_hex(0xAAB1BE), 0);
    view_context.time_sync_qr = lv_canvas_create(view_context.page_content);
    LV_DRAW_BUF_INIT_STATIC(time_sync_qr_buffer);
    lv_canvas_set_draw_buf(view_context.time_sync_qr,
                           &time_sync_qr_buffer);
    lv_canvas_set_palette(view_context.time_sync_qr, 0,
                          lv_color32_make(0xFF, 0xFF, 0xFF, 0xFF));
    lv_canvas_set_palette(view_context.time_sync_qr, 1,
                          lv_color32_make(0x00, 0x00, 0x00, 0xFF));
    lv_obj_add_flag(view_context.time_sync_qr, LV_OBJ_FLAG_HIDDEN);
    view_context.statistics_metrics = lv_obj_create(view_context.page_content);
    lv_obj_set_size(view_context.statistics_metrics, LV_PCT(100), 86);
    lv_obj_set_style_bg_opa(view_context.statistics_metrics, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(view_context.statistics_metrics, 0, 0);
    lv_obj_set_style_pad_all(view_context.statistics_metrics, 0, 0);
    lv_obj_set_flex_flow(view_context.statistics_metrics, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(view_context.statistics_metrics, 10, 0);
    view_context.statistics_focus_card = lv_obj_create(
        view_context.statistics_metrics);
    view_context.statistics_goal_card = lv_obj_create(
        view_context.statistics_metrics);
    lv_obj_set_height(view_context.statistics_focus_card, LV_PCT(100));
    lv_obj_set_height(view_context.statistics_goal_card, LV_PCT(100));
    lv_obj_set_flex_grow(view_context.statistics_focus_card, 1);
    lv_obj_set_flex_grow(view_context.statistics_goal_card, 1);
    lv_obj_set_style_bg_color(view_context.statistics_focus_card,
                              lv_color_hex(0x1A1F27), 0);
    lv_obj_set_style_bg_color(view_context.statistics_goal_card,
                              lv_color_hex(0x1A1F27), 0);
    lv_obj_set_style_bg_opa(view_context.statistics_focus_card,
                            LV_OPA_COVER, 0);
    lv_obj_set_style_bg_opa(view_context.statistics_goal_card,
                            LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(view_context.statistics_focus_card, 0, 0);
    lv_obj_set_style_border_width(view_context.statistics_goal_card, 0, 0);
    lv_obj_set_style_pad_all(view_context.statistics_focus_card, 12, 0);
    lv_obj_set_style_pad_all(view_context.statistics_goal_card, 12, 0);
    lv_obj_set_style_radius(view_context.statistics_focus_card, 12, 0);
    lv_obj_set_style_radius(view_context.statistics_goal_card, 12, 0);
    view_context.statistics_focus_value = lv_label_create(
        view_context.statistics_focus_card);
    lv_label_set_recolor(view_context.statistics_focus_value, true);
    lv_obj_align(view_context.statistics_focus_value, LV_ALIGN_TOP_LEFT, 0, 0);
    view_context.statistics_goal_value = lv_label_create(
        view_context.statistics_goal_card);
    lv_label_set_recolor(view_context.statistics_goal_value, true);
    lv_obj_align(view_context.statistics_goal_value, LV_ALIGN_TOP_LEFT, 0, 0);
    view_context.statistics_goal_bar = lv_bar_create(
        view_context.statistics_goal_card);
    lv_obj_set_size(view_context.statistics_goal_bar, LV_PCT(100), 10);
    lv_obj_align(view_context.statistics_goal_bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_bar_set_range(view_context.statistics_goal_bar, 0, 100);
    lv_obj_set_style_bg_color(view_context.statistics_goal_bar,
                              lv_color_hex(0x303846), LV_PART_MAIN);
    lv_obj_set_style_bg_color(view_context.statistics_goal_bar,
                              lv_color_hex(SETTINGS_VALUE_TEXT_COLOR),
                              LV_PART_INDICATOR);
    lv_obj_add_flag(view_context.statistics_metrics, LV_OBJ_FLAG_HIDDEN);
    view_context.settings_switches = lv_obj_create(view_context.page_content);
    lv_obj_set_size(view_context.settings_switches, LV_PCT(100),
                    LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(view_context.settings_switches, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(view_context.settings_switches, 0, 0);
    lv_obj_set_style_pad_all(view_context.settings_switches, 0, 0);
    lv_obj_set_style_pad_row(view_context.settings_switches, 8, 0);
    lv_obj_set_flex_flow(view_context.settings_switches,
                         LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_text_color(view_context.settings_switches,
                                lv_color_hex(0xF5F7FA), 0);
    (void) make_settings_section_label(view_context.settings_switches,
                                       "STATISTICS", false);
    (void) make_setting_action_row(
        view_context.settings_switches, "Today's statistics",
        POMODORO_APPLICATION_ACTION_OPEN_STATISTICS,
        &view_context.settings_today_value);
    (void) make_settings_section_label(view_context.settings_switches,
                                       "TIMER", true);
    (void) make_setting_action_row(
        view_context.settings_switches, "Focus duration",
        POMODORO_APPLICATION_ACTION_OPEN_FOCUS_MINUTES,
        &view_context.settings_timer_values[0]);
    (void) make_setting_action_row(
        view_context.settings_switches, "Short break duration",
        POMODORO_APPLICATION_ACTION_OPEN_SHORT_BREAK_MINUTES,
        &view_context.settings_timer_values[1]);
    (void) make_setting_action_row(
        view_context.settings_switches, "Long break duration",
        POMODORO_APPLICATION_ACTION_OPEN_LONG_BREAK_MINUTES,
        &view_context.settings_timer_values[2]);
    (void) make_setting_action_row(
        view_context.settings_switches, "Daily goal",
        POMODORO_APPLICATION_ACTION_OPEN_DAILY_GOAL,
        &view_context.settings_timer_values[3]);
    (void) make_settings_section_label(view_context.settings_switches,
                                       "AUTOMATION", true);
    (void) make_setting_switch_row(
        view_context.settings_switches, "Auto-start breaks",
        POMODORO_APPLICATION_ACTION_TOGGLE_AUTO_BREAK,
        &view_context.auto_breaks_switch);
    (void) make_setting_switch_row(
        view_context.settings_switches, "Auto-start focus",
        POMODORO_APPLICATION_ACTION_TOGGLE_AUTO_FOCUS,
        &view_context.auto_focus_switch);
    (void) make_settings_section_label(view_context.settings_switches,
                                       "SOUND", true);
    (void) make_setting_switch_row(
        view_context.settings_switches, "Sound",
        POMODORO_APPLICATION_ACTION_TOGGLE_SOUND,
        &view_context.sound_switch);
    view_context.settings_volume_value = lv_label_create(
        view_context.settings_switches);
    lv_obj_set_style_text_color(
        view_context.settings_volume_value,
        lv_color_hex(SETTINGS_VALUE_TEXT_COLOR), 0);
    (void) make_settings_section_label(view_context.settings_switches,
                                       "DISPLAY", true);
    (void) make_setting_action_row(
        view_context.settings_switches, "Brightness",
        POMODORO_APPLICATION_ACTION_OPEN_BRIGHTNESS,
        &view_context.settings_brightness_value);
    lv_obj_add_flag(view_context.settings_switches, LV_OBJ_FLAG_HIDDEN);
    view_context.statistics_chart = lv_obj_create(view_context.page_content);
    lv_obj_set_size(view_context.statistics_chart, LV_PCT(100), 180);
    lv_obj_set_style_bg_opa(view_context.statistics_chart, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(view_context.statistics_chart, 0, 0);
    lv_obj_set_style_pad_all(view_context.statistics_chart, 0, 0);
    for (uint8_t index = 0; index < 7U; index++) {
        int32_t x = 10 + (int32_t) index * 61;
        view_context.statistics_bars[index] = lv_bar_create(
            view_context.statistics_chart);
        lv_obj_set_size(view_context.statistics_bars[index], 30, 116);
        lv_obj_align(view_context.statistics_bars[index],
                     LV_ALIGN_BOTTOM_LEFT, x, -28);
        lv_bar_set_orientation(view_context.statistics_bars[index],
                               LV_BAR_ORIENTATION_VERTICAL);
        view_context.statistics_dates[index] = lv_label_create(
            view_context.statistics_chart);
        lv_obj_set_style_text_color(
            view_context.statistics_dates[index],
            lv_color_hex(SETTINGS_SECTION_TEXT_COLOR), 0);
        lv_obj_align(view_context.statistics_dates[index],
                     LV_ALIGN_BOTTOM_LEFT, x - 2, -2);
        view_context.statistics_counts[index] = lv_label_create(
            view_context.statistics_chart);
        lv_obj_set_style_text_color(
            view_context.statistics_counts[index],
            lv_color_hex(SETTINGS_VALUE_TEXT_COLOR), 0);
        lv_obj_align(view_context.statistics_counts[index],
                     LV_ALIGN_TOP_LEFT, x + 8, 0);
    }
    lv_obj_add_flag(view_context.statistics_chart, LV_OBJ_FLAG_HIDDEN);
    view_context.numeric_roller = lv_roller_create(view_context.page_content);
    lv_obj_set_size(view_context.numeric_roller, LV_PCT(70), 168);
    lv_roller_set_visible_row_count(view_context.numeric_roller, 3);
    lv_obj_add_event_cb(view_context.numeric_roller, numeric_roller_changed,
                        LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_flag(view_context.numeric_roller, LV_OBJ_FLAG_HIDDEN);
    (void) make_settings_section_label(view_context.settings_switches,
                                       "DATE & TIME", true);
    (void) make_setting_action_row(
        view_context.settings_switches, "Sync from iPhone",
        POMODORO_APPLICATION_ACTION_OPEN_TIME_SYNC,
        &view_context.settings_time_sync_value);
    (void) make_setting_action_row(
        view_context.settings_switches, "Set manually",
        POMODORO_APPLICATION_ACTION_OPEN_DATE_TIME,
        &view_context.settings_datetime_value);
    (void) make_settings_section_label(view_context.settings_switches,
                                       "POWER", true);
    (void) make_setting_action_row(
        view_context.settings_switches, "Power off",
        POMODORO_APPLICATION_ACTION_POWER_OFF,
        &view_context.settings_power_value);
    view_context.auxiliary_buttons_container = lv_obj_create(
        view_context.page_content);
    lv_obj_set_width(view_context.auxiliary_buttons_container, LV_PCT(100));
    lv_obj_set_height(view_context.auxiliary_buttons_container,
                      LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(view_context.auxiliary_buttons_container,
                            LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(view_context.auxiliary_buttons_container,
                                  0, 0);
    lv_obj_set_style_pad_all(view_context.auxiliary_buttons_container, 0, 0);
    lv_obj_set_style_pad_row(view_context.auxiliary_buttons_container, 8, 0);
    lv_obj_set_style_pad_column(view_context.auxiliary_buttons_container, 8, 0);
    lv_obj_set_flex_flow(view_context.auxiliary_buttons_container,
                         LV_FLEX_FLOW_ROW_WRAP);
    for (uint8_t index = 0; index < AUXILIARY_BUTTON_COUNT; index++) {
        auxiliary_button_t *entry = &view_context.auxiliary_buttons[index];
        entry->action = auxiliary_button_definitions[index].action;
        entry->button = make_action_button(
            view_context.auxiliary_buttons_container, entry->action,
            auxiliary_button_definitions[index].text);
        entry->label = lv_obj_get_child(entry->button, 0);
        bool menu_destination =
            entry->action == POMODORO_APPLICATION_ACTION_OPEN_STATISTICS ||
            entry->action == POMODORO_APPLICATION_ACTION_OPEN_SETTINGS;
        lv_obj_set_size(entry->button, menu_destination ? 420 : 136,
                        menu_destination ? 56 : 42);
        if (menu_destination) {
            lv_obj_set_style_text_color(entry->label,
                                        lv_color_hex(0xF5F7FA), 0);
        }
    }
    lv_obj_add_flag(view_context.page_panel, LV_OBJ_FLAG_HIDDEN);

    view_context.confirmation_mask = lv_obj_create(screen);
    lv_obj_set_size(view_context.confirmation_mask, 480, 480);
    lv_obj_align(view_context.confirmation_mask, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(view_context.confirmation_mask,
                              lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(view_context.confirmation_mask, LV_OPA_60, 0);
    lv_obj_set_style_border_width(view_context.confirmation_mask, 0, 0);
    lv_obj_set_style_radius(view_context.confirmation_mask, 0, 0);
    lv_obj_remove_flag(view_context.confirmation_mask, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(view_context.confirmation_mask, confirmation_cancelled,
                        LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(view_context.confirmation_mask, LV_OBJ_FLAG_HIDDEN);

    view_context.confirmation_overlay = lv_obj_create(screen);
    lv_obj_set_size(view_context.confirmation_overlay, 432, 250);
    lv_obj_align(view_context.confirmation_overlay, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(view_context.confirmation_overlay,
                              lv_color_hex(0x171B22), 0);
    lv_obj_set_style_radius(view_context.confirmation_overlay, 24, 0);
    view_context.confirmation_title = lv_label_create(
        view_context.confirmation_overlay);
    lv_obj_set_style_text_font(view_context.confirmation_title,
                               &lv_font_montserrat_24, 0);
    lv_obj_align(view_context.confirmation_title, LV_ALIGN_TOP_MID, 0, 22);
    view_context.confirmation_body = lv_label_create(
        view_context.confirmation_overlay);
    lv_label_set_text(view_context.confirmation_body,
                      "Current progress will be discarded.");
    lv_obj_set_style_text_color(view_context.confirmation_body,
                                lv_color_hex(0xAAB1BE), 0);
    lv_obj_align(view_context.confirmation_body, LV_ALIGN_TOP_MID, 0, 66);
    lv_obj_t *cancel = make_text_button(view_context.confirmation_overlay,
                                        confirmation_cancelled, "Cancel");
    lv_obj_set_size(cancel, 150, 50);
    lv_obj_align(cancel, LV_ALIGN_BOTTOM_LEFT, 30, -25);
    lv_obj_t *confirm = make_text_button(view_context.confirmation_overlay,
                                         confirmation_accepted, "Confirm");
    view_context.confirmation_button_label = lv_obj_get_child(confirm, 0);
    lv_obj_set_size(confirm, 180, 50);
    lv_obj_set_style_bg_color(confirm, lv_color_hex(0xD94B4B), 0);
    lv_obj_align(confirm, LV_ALIGN_BOTTOM_RIGHT, -30, -25);
    lv_obj_add_flag(view_context.confirmation_overlay, LV_OBJ_FLAG_HIDDEN);

    view_context.mode_transition_overlay = lv_obj_create(screen);
    lv_obj_set_size(view_context.mode_transition_overlay, 480, 480);
    lv_obj_align(view_context.mode_transition_overlay, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(view_context.mode_transition_overlay,
                              lv_color_hex(0x07090D), 0);
    lv_obj_set_style_bg_opa(view_context.mode_transition_overlay,
                            LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(view_context.mode_transition_overlay, 0, 0);
    lv_obj_set_style_radius(view_context.mode_transition_overlay, 0, 0);
    lv_obj_remove_flag(view_context.mode_transition_overlay,
                       LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(view_context.mode_transition_overlay, LV_OBJ_FLAG_HIDDEN);

    view_context.status_overlay = lv_obj_create(screen);
    lv_obj_remove_style_all(view_context.status_overlay);
    lv_obj_set_size(view_context.status_overlay, 220, LV_SIZE_CONTENT);
    lv_obj_align(view_context.status_overlay, LV_ALIGN_TOP_MID, 0, 62);
    lv_obj_set_flex_flow(view_context.status_overlay, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(view_context.status_overlay, 8, 0);
    lv_obj_remove_flag(view_context.status_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(view_context.status_overlay, LV_OBJ_FLAG_SCROLLABLE);
    view_context.rtc_warning = lv_label_create(view_context.status_overlay);
    view_context.battery_warning = lv_label_create(view_context.status_overlay);
    view_context.volume_feedback = lv_obj_create(view_context.status_overlay);
    lv_obj_set_size(view_context.volume_feedback, LV_PCT(100), 42);
    lv_obj_set_style_bg_color(view_context.volume_feedback,
                              lv_color_hex(0x1A1F27), 0);
    lv_obj_set_style_border_width(view_context.volume_feedback, 0, 0);
    lv_obj_set_style_radius(view_context.volume_feedback, 12, 0);
    lv_obj_set_style_pad_all(view_context.volume_feedback, 8, 0);
    lv_obj_set_flex_flow(view_context.volume_feedback, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(view_context.volume_feedback, 8, 0);
    lv_obj_set_flex_align(view_context.volume_feedback,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    view_context.volume_icon = lv_label_create(view_context.volume_feedback);
    lv_obj_set_style_text_color(view_context.volume_icon,
                                lv_color_hex(SETTINGS_ITEM_TEXT_COLOR), 0);
    for (uint8_t index = 0; index < 5U; index++) {
        view_context.volume_segments[index] = lv_obj_create(
            view_context.volume_feedback);
        lv_obj_remove_style_all(view_context.volume_segments[index]);
        lv_obj_set_size(view_context.volume_segments[index], 14, 8);
        lv_obj_set_style_radius(view_context.volume_segments[index], 3, 0);
    }
    view_context.volume_value = lv_label_create(view_context.volume_feedback);
    lv_obj_set_style_text_color(view_context.volume_value,
                                lv_color_hex(SETTINGS_VALUE_TEXT_COLOR), 0);
    lv_obj_set_style_text_align(view_context.status_overlay,
                                LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_flag(view_context.rtc_warning, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(view_context.battery_warning, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(view_context.volume_feedback, LV_OBJ_FLAG_HIDDEN);

    lvgl_port_unlock();
    return ESP_OK;
}

static void render_progress(const pomodoro_application_view_t *view,
                            lv_color_t accent)
{
    lv_color_t focus_accent = lv_color_hex(0xFF5C5C);
    lv_label_set_text_fmt(view_context.set_label, "Set %u/4",
                          view->set_progress);
    for (uint8_t index = 0; index < 4U; index++) {
        bool complete = index < view->set_progress;
        lv_obj_set_style_border_color(view_context.set_markers[index],
                                      focus_accent, 0);
        lv_obj_set_style_bg_opa(view_context.set_markers[index],
                                complete ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_set_style_bg_color(view_context.set_markers[index],
                                  focus_accent, 0);
    }
    for (uint8_t index = 0; index < 3U; index++) {
        lv_obj_set_style_bg_opa(view_context.mode_dots[index], LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(
            view_context.mode_dots[index],
            index == (uint8_t) view->mode ? accent : lv_color_hex(0x454B56), 0);
    }
}

static void render_completion(const pomodoro_application_view_t *view)
{
    bool focus = view->mode == POMODORO_MODE_FOCUS;
    mode_presentation_t scheduled = mode_presentation(view->scheduled_mode);
    lv_obj_remove_flag(view_context.completion_group, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(view_context.ring, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(view_context.time, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(view_context.status, LV_OBJ_FLAG_HIDDEN);
    if (focus) {
        lv_obj_remove_flag(view_context.tomato, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(view_context.cup, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(view_context.cup_handle, LV_OBJ_FLAG_HIDDEN);
        for (uint8_t index = 0; index < 2U; index++) {
            lv_obj_add_flag(view_context.cup_steam[index], LV_OBJ_FLAG_HIDDEN);
        }
        lv_label_set_text(view_context.completion_title, "Focus complete");
        lv_label_set_text(view_context.completion_body,
                          view->daily_goal_reached
                              ? "Great work! +1\nDaily goal reached!"
                              : "Great work! +1");
    } else {
        lv_obj_add_flag(view_context.tomato, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(view_context.cup, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(view_context.cup_handle, LV_OBJ_FLAG_HIDDEN);
        for (uint8_t index = 0; index < 2U; index++) {
            lv_obj_remove_flag(view_context.cup_steam[index],
                               LV_OBJ_FLAG_HIDDEN);
        }
        lv_label_set_text(view_context.completion_title, "Break over");
        lv_label_set_text(view_context.completion_body,
                          "Ready for another focus?");
    }
    for (uint8_t index = 0; index < COMPLETION_CONFETTI_COUNT; index++) {
        if (view->daily_goal_reached) {
            lv_obj_remove_flag(view_context.confetti[index], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(view_context.confetti[index], LV_OBJ_FLAG_HIDDEN);
        }
    }
    lv_obj_align(view_context.completion_title, LV_ALIGN_TOP_MID, 0,
                 view->daily_goal_reached ? 74 : 82);
    lv_obj_align(view_context.completion_body, LV_ALIGN_TOP_MID, 0,
                 view->daily_goal_reached ? 108 : 118);
    lv_obj_align(view_context.completion_countdown, LV_ALIGN_TOP_MID, 0,
                 view->daily_goal_reached ? 154 : 150);
    if (view->auto_start_pending) {
        lv_obj_align(view_context.completion_group, LV_ALIGN_CENTER, 0,
                     COMPLETION_AUTO_GROUP_Y);
        lv_label_set_text_fmt(view_context.completion_countdown,
                              "%s starts in %u",
                              scheduled.name,
                              view->auto_start_seconds_remaining);
        lv_label_set_text(view_context.primary_label, "Start now");
        lv_label_set_text(view_context.secondary_label, "Cancel auto-start");
        lv_obj_remove_flag(view_context.secondary_button, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_align(view_context.completion_group, LV_ALIGN_CENTER, 0,
                     COMPLETION_MANUAL_GROUP_Y);
        lv_label_set_text(view_context.completion_countdown, "");
        if (focus) {
            lv_label_set_text(view_context.primary_label, "Start break");
        } else {
            lv_label_set_text_fmt(view_context.primary_label, "Start %s",
                                  scheduled.action_name);
        }
        lv_obj_add_flag(view_context.secondary_button, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_remove_flag(view_context.primary_button, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_size(view_context.primary_button, 220, 56);
    lv_obj_align(view_context.primary_button, LV_ALIGN_CENTER, 0,
                 view->auto_start_pending ? COMPLETION_AUTO_PRIMARY_Y :
                                            COMPLETION_MANUAL_PRIMARY_Y);
    if (view->auto_start_pending) {
        lv_obj_set_size(view_context.secondary_button, 170, 44);
        lv_obj_align(view_context.secondary_button, LV_ALIGN_CENTER, 0,
                     COMPLETION_AUTO_SECONDARY_Y);
    }
}

static void set_auxiliary_button_visible(
    pomodoro_application_action_t action, bool visible)
{
    for (uint8_t index = 0; index < AUXILIARY_BUTTON_COUNT; index++) {
        auxiliary_button_t *entry = &view_context.auxiliary_buttons[index];
        if (entry->action == action) {
            if (visible) {
                lv_obj_remove_flag(entry->button, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(entry->button, LV_OBJ_FLAG_HIDDEN);
            }
            return;
        }
    }
}

static void set_auxiliary_button_text(
    pomodoro_application_action_t action, const char *text)
{
    for (uint8_t index = 0; index < AUXILIARY_BUTTON_COUNT; index++) {
        auxiliary_button_t *entry = &view_context.auxiliary_buttons[index];
        if (entry->action == action) {
            lv_label_set_text(entry->label, text);
            return;
        }
    }
}

static void hide_auxiliary_buttons(void)
{
    for (uint8_t index = 0; index < AUXILIARY_BUTTON_COUNT; index++) {
        lv_obj_add_flag(view_context.auxiliary_buttons[index].button,
                        LV_OBJ_FLAG_HIDDEN);
    }
}

static const char *numeric_setting_name(
    pomodoro_application_numeric_setting_t setting)
{
    switch (setting) {
    case POMODORO_APPLICATION_SETTING_SHORT_BREAK_MINUTES:
        return "Short break";
    case POMODORO_APPLICATION_SETTING_LONG_BREAK_MINUTES:
        return "Long break";
    case POMODORO_APPLICATION_SETTING_DAILY_GOAL:
        return "Daily goal";
    case POMODORO_APPLICATION_SETTING_BRIGHTNESS_PERCENT:
        return "Brightness";
    default:
        return "Focus";
    }
}

static void render_auxiliary_page(const pomodoro_application_view_t *view)
{
    hide_auxiliary_buttons();
    lv_obj_remove_flag(view_context.page_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(view_context.menu_button, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(view_context.page_content, LV_OBJ_FLAG_SCROLLABLE);
    if (view->page != POMODORO_APPLICATION_PAGE_SETTINGS) {
        lv_obj_scroll_to_y(view_context.page_content, 0, LV_ANIM_OFF);
    }
    lv_obj_add_flag(view_context.statistics_chart, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(view_context.statistics_metrics, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(view_context.settings_switches, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(view_context.numeric_roller, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(view_context.time_sync_qr, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(view_context.page_back_button, LV_OBJ_FLAG_HIDDEN);
    if (view->phase != POMODORO_APPLICATION_PHASE_TIME_SETUP ||
        view->time_setup_stage != POMODORO_APPLICATION_TIME_SETUP_SYNC ||
        view->time_sync_session.join_uri[0] == '\0') {
        clear_time_sync_qr();
    }

    if (view->phase == POMODORO_APPLICATION_PHASE_TIME_SETUP) {
        if (view->time_setup_stage ==
            POMODORO_APPLICATION_TIME_SETUP_LANDING) {
            lv_label_set_text(view_context.page_title, "Set date & time");
            lv_label_set_text(
                view_context.page_body,
                "Time is required for daily statistics and timer recovery.\n\n"
                "No account or internet connection is required.");
            set_auxiliary_button_text(
                POMODORO_APPLICATION_ACTION_OPEN_DATE_TIME, "Set manually");
            set_auxiliary_button_visible(
                POMODORO_APPLICATION_ACTION_OPEN_TIME_SYNC, true);
            set_auxiliary_button_visible(
                POMODORO_APPLICATION_ACTION_OPEN_DATE_TIME, true);
            lv_obj_add_flag(view_context.page_back_button,
                            LV_OBJ_FLAG_HIDDEN);
            return;
        }
        if (view->time_setup_stage ==
            POMODORO_APPLICATION_TIME_SETUP_SYNC) {
            char text[384];
            if (view->time_sync_failed) {
                snprintf(text, sizeof(text),
                         "Couldn't sync time.\n\nTry again or set the time "
                         "manually.");
                set_auxiliary_button_text(
                    POMODORO_APPLICATION_ACTION_OPEN_TIME_SYNC,
                    "Try again");
                set_auxiliary_button_visible(
                    POMODORO_APPLICATION_ACTION_OPEN_TIME_SYNC, true);
            } else {
                snprintf(
                    text, sizeof(text),
                    "%s\nPassword: %s\n\n%s\nOpen http://192.168.4.1 in "
                    "Safari.\nExpires in %u:%02u.",
                    view->time_sync_session.ssid,
                    view->time_sync_session.password,
                    view->time_sync_connected
                        ? "Connected — confirm Sync to Pomodoro on your iPhone."
                        : "Scan this code with your iPhone.",
                    view->time_sync_seconds_remaining / 60U,
                    view->time_sync_seconds_remaining % 60U);
                render_time_sync_qr(view->time_sync_session.join_uri);
                lv_obj_remove_flag(view_context.time_sync_qr,
                                   LV_OBJ_FLAG_HIDDEN);
            }
            lv_label_set_text(view_context.page_title, "Sync from iPhone");
            lv_label_set_text(view_context.page_body, text);
            set_auxiliary_button_text(
                POMODORO_APPLICATION_ACTION_OPEN_DATE_TIME,
                "Set manually instead");
            set_auxiliary_button_visible(
                POMODORO_APPLICATION_ACTION_OPEN_DATE_TIME, true);
            return;
        }
        if (view->time_setup_stage ==
            POMODORO_APPLICATION_TIME_SETUP_SYNC_SUCCESS) {
            lv_label_set_text(view_context.page_title, "Time synced");
            lv_label_set_text(view_context.page_body,
                              LV_SYMBOL_OK "\n\nTime and daylight-saving "
                              "rules saved.");
            lv_obj_add_flag(view_context.page_back_button,
                            LV_OBJ_FLAG_HIDDEN);
            return;
        }
        char text[256];
        snprintf(
            text, sizeof(text),
            "%04u-%02u-%02u  %02u:%02u\nUTC%+.2f\n\n"
            "Set the local date, time, and fixed UTC offset.",
            view->manual_time_draft.local_date.year,
            view->manual_time_draft.local_date.month,
            view->manual_time_draft.local_date.day,
            view->manual_time_draft.hour, view->manual_time_draft.minute,
            (double) view->manual_time_draft.utc_offset_quarter_hours / 4.0);
        lv_label_set_text(view_context.page_title, "Set date & time");
        lv_label_set_text(view_context.page_body, text);
        for (pomodoro_application_action_t action =
                 POMODORO_APPLICATION_ACTION_MANUAL_YEAR_DOWN;
             action <= POMODORO_APPLICATION_ACTION_MANUAL_TIME_CONFIRM;
             action++) {
            set_auxiliary_button_visible(action, true);
        }
        return;
    }
    if (view->page == POMODORO_APPLICATION_PAGE_MENU) {
        lv_label_set_text(view_context.page_title, "Menu");
        lv_label_set_text(view_context.page_body, "");
        set_auxiliary_button_visible(
            POMODORO_APPLICATION_ACTION_OPEN_STATISTICS, true);
        set_auxiliary_button_visible(
            POMODORO_APPLICATION_ACTION_OPEN_SETTINGS, true);
        return;
    }
    if (view->page == POMODORO_APPLICATION_PAGE_STATISTICS) {
        char text[768];
        lv_label_set_recolor(view_context.page_body, true);
        snprintf(text, sizeof(text),
                 "#AAB1BE TODAY#  #9FC2FF ×%u#",
                 view->today_completed_pomodoros);
        if (!view->statistics_available) {
            snprintf(text, sizeof(text),
                     "Statistics need a valid date and time.\n\n"
                     "Open date/time setup to restore the chart.");
            set_auxiliary_button_visible(
                POMODORO_APPLICATION_ACTION_OPEN_DATE_TIME, true);
        } else {
            lv_label_set_text_fmt(view_context.statistics_focus_value,
                                  "#AAB1BE FOCUS TIME#\n#9FC2FF %" PRIu32
                                  " min#",
                                  view->today_completed_focus_seconds / 60U);
            lv_label_set_text_fmt(view_context.statistics_goal_value,
                                  "#AAB1BE GOAL#\n#9FC2FF %u%%#",
                                  view->today_goal_percent);
            lv_bar_set_value(view_context.statistics_goal_bar,
                             view->today_goal_fill_percent, LV_ANIM_OFF);
            lv_obj_remove_flag(view_context.statistics_metrics,
                               LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(view_context.statistics_chart,
                               LV_OBJ_FLAG_HIDDEN);
            for (uint8_t index = 0; index < 7U; index++) {
                const pomodoro_application_statistics_bar_t *bar =
                    &view->trailing_seven_days[index];
                lv_bar_set_range(view_context.statistics_bars[index], 0,
                                 view->statistics_chart_scale);
                lv_bar_set_value(view_context.statistics_bars[index],
                                 bar->completed_pomodoros, LV_ANIM_OFF);
                lv_obj_set_style_bg_color(
                    view_context.statistics_bars[index],
                    lv_color_hex(bar->is_today
                        ? SETTINGS_VALUE_TEXT_COLOR : 0x5B7396),
                    LV_PART_INDICATOR);
                lv_label_set_text_fmt(view_context.statistics_dates[index],
                                      "%u/%u", bar->date.month,
                                      bar->date.day);
                lv_label_set_text_fmt(view_context.statistics_counts[index],
                                      "%u", bar->completed_pomodoros);
                int32_t count_y = -32 -
                    ((int32_t) bar->completed_pomodoros * 116 /
                     view->statistics_chart_scale);
                lv_obj_align(view_context.statistics_counts[index],
                             LV_ALIGN_BOTTOM_LEFT,
                             18 + (int32_t) index * 61, count_y);
            }
        }
        lv_label_set_text(view_context.page_title, "Statistics");
        lv_label_set_text(view_context.page_body, text);
        return;
    }
    lv_label_set_recolor(view_context.page_body, false);
    if (view->page == POMODORO_APPLICATION_PAGE_NUMERIC_STEPPER) {
        char text[80];
        bool is_brightness = view->numeric_setting ==
            POMODORO_APPLICATION_SETTING_BRIGHTNESS_PERCENT;
        snprintf(text, sizeof(text),
                 is_brightness ? "%u%%" : "Range %u-%u - step %u",
                 is_brightness ? view->numeric_draft_value :
                                 view->numeric_minimum,
                 view->numeric_maximum, view->numeric_step);
        lv_label_set_text(view_context.page_title,
                          numeric_setting_name(view->numeric_setting));
        lv_label_set_text(view_context.page_body, text);
        set_auxiliary_button_text(
            POMODORO_APPLICATION_ACTION_NUMERIC_DECREMENT,
            is_brightness ? "-" : LV_SYMBOL_DOWN);
        set_auxiliary_button_text(
            POMODORO_APPLICATION_ACTION_NUMERIC_INCREMENT,
            is_brightness ? "+" : LV_SYMBOL_UP);
        char options[256] = "";
        size_t used = 0U;
        for (uint16_t value = view->numeric_minimum;
             value <= view->numeric_maximum; value += view->numeric_step) {
            int written = snprintf(options + used, sizeof(options) - used,
                                   value == view->numeric_maximum ? "%u" :
                                                                    "%u\n",
                                   value);
            if (written < 0 || (size_t) written >= sizeof(options) - used) {
                break;
            }
            used += (size_t) written;
        }
        if (!is_brightness) {
            view_context.numeric_roller_rendering = true;
            lv_roller_set_options(view_context.numeric_roller, options,
                                  LV_ROLLER_MODE_NORMAL);
            lv_roller_set_selected(
                view_context.numeric_roller,
                (view->numeric_draft_value - view->numeric_minimum) /
                    view->numeric_step,
                LV_ANIM_OFF);
            view_context.numeric_roller_rendering = false;
            lv_obj_remove_flag(view_context.numeric_roller,
                               LV_OBJ_FLAG_HIDDEN);
        }
        set_auxiliary_button_visible(
            POMODORO_APPLICATION_ACTION_NUMERIC_DECREMENT, true);
        set_auxiliary_button_visible(
            POMODORO_APPLICATION_ACTION_NUMERIC_INCREMENT, true);
        set_auxiliary_button_visible(
            POMODORO_APPLICATION_ACTION_NUMERIC_CONFIRM, true);
        return;
    }

    lv_label_set_text(view_context.page_body, "");
    lv_label_set_text_fmt(view_context.settings_today_value, "%u / %u",
                          view->today_completed_pomodoros,
                          view->settings.daily_goal);
    lv_label_set_text_fmt(view_context.settings_timer_values[0], "%u min",
                          view->settings.focus_minutes);
    lv_label_set_text_fmt(view_context.settings_timer_values[1], "%u min",
                          view->settings.short_break_minutes);
    lv_label_set_text_fmt(view_context.settings_timer_values[2], "%u min",
                          view->settings.long_break_minutes);
    lv_label_set_text_fmt(view_context.settings_timer_values[3], "%u",
                          view->settings.daily_goal);
    lv_label_set_text_fmt(
        view_context.settings_volume_value,
        "Volume %u/5 - Use the side buttons to adjust volume",
        view->settings.volume);
    lv_label_set_text_fmt(view_context.settings_brightness_value, "%u%%",
                          view->settings.brightness_percent);
    lv_label_set_text_fmt(
        view_context.settings_time_sync_value, "%s%s",
        view->time_zone_needs_sync ? "Sync soon - " : "",
        view->time_zone_needs_sync ? "rules expiring" :
        view->time_zone_name[0] != '\0' ? view->time_zone_name :
                                         "automatic DST");
    lv_label_set_text_fmt(
        view_context.settings_datetime_value,
        "%04u-%02u-%02u %02u:%02u UTC%+.2f",
        view->current_local_date.year, view->current_local_date.month,
        view->current_local_date.day, view->current_local_hour,
        view->current_local_minute,
        (double) view->settings.utc_offset_quarter_hours / 4.0);
    lv_label_set_text_fmt(
        view_context.settings_power_value, "%s%s%u%%",
        view->device_status.usb_power ? "USB power  " : "",
        view->device_status.charging ? "Charging  " : "",
        view->device_status.battery_percent);
    lv_label_set_text(view_context.page_title, "Settings");
    lv_obj_remove_flag(view_context.settings_switches, LV_OBJ_FLAG_HIDDEN);
    if (view->settings.auto_start_breaks) {
        lv_obj_add_state(view_context.auto_breaks_switch, LV_STATE_CHECKED);
    } else {
        lv_obj_remove_state(view_context.auto_breaks_switch, LV_STATE_CHECKED);
    }
    if (view->settings.auto_start_focus) {
        lv_obj_add_state(view_context.auto_focus_switch, LV_STATE_CHECKED);
    } else {
        lv_obj_remove_state(view_context.auto_focus_switch, LV_STATE_CHECKED);
    }
    if (view->settings.volume != 0U) {
        lv_obj_add_state(view_context.sound_switch, LV_STATE_CHECKED);
    } else {
        lv_obj_remove_state(view_context.sound_switch, LV_STATE_CHECKED);
    }
    lv_obj_add_flag(view_context.page_content, LV_OBJ_FLAG_SCROLLABLE);
}

static void render_status_overlay(const pomodoro_application_view_t *view)
{
    if (view->rtc_is_valid) {
        lv_obj_add_flag(view_context.rtc_warning, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_label_set_text(view_context.rtc_warning,
                          "Date & time unavailable");
        lv_obj_remove_flag(view_context.rtc_warning, LV_OBJ_FLAG_HIDDEN);
    }
    const char *battery_icon = view->device_status.battery_percent <= 10U
        ? LV_SYMBOL_BATTERY_EMPTY
        : view->device_status.battery_percent <= 30U
            ? LV_SYMBOL_BATTERY_1
            : view->device_status.battery_percent <= 55U
                ? LV_SYMBOL_BATTERY_2
                : view->device_status.battery_percent <= 80U
                    ? LV_SYMBOL_BATTERY_3
                    : LV_SYMBOL_BATTERY_FULL;
    lv_label_set_text_fmt(
        view_context.top_power_status, "%s%s%s %u%%",
        view->device_status.usb_power ? LV_SYMBOL_USB : "",
        view->device_status.charging ? LV_SYMBOL_CHARGE : "",
        battery_icon, view->device_status.battery_percent);
    lv_obj_set_style_text_color(
        view_context.top_power_status,
        lv_color_hex(view->device_status.low_battery ? 0xFFB74D : 0xF5F7FA),
        0);
    if (view->low_battery_warning_visible) {
        lv_label_set_text(view_context.battery_warning, "Battery low");
        lv_obj_remove_flag(view_context.battery_warning, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(view_context.battery_warning, LV_OBJ_FLAG_HIDDEN);
    }
    if (view->volume_feedback_visible) {
        lv_label_set_text(view_context.volume_icon,
                          view->settings.volume == 0U ? LV_SYMBOL_MUTE :
                                                        LV_SYMBOL_VOLUME_MID);
        for (uint8_t index = 0; index < 5U; index++) {
            lv_obj_set_style_bg_color(
                view_context.volume_segments[index],
                lv_color_hex(index < view->settings.volume
                    ? SETTINGS_VALUE_TEXT_COLOR :
                                                            0x555D6A),
                0);
            lv_obj_set_style_bg_opa(view_context.volume_segments[index],
                                    LV_OPA_COVER, 0);
        }
        lv_label_set_text_fmt(view_context.volume_value, "%u/5",
                              view->settings.volume);
        lv_obj_remove_flag(view_context.volume_feedback, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(view_context.volume_feedback, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_move_foreground(view_context.status_overlay);
    if (view->confirmation != POMODORO_APPLICATION_CONFIRM_NONE) {
        lv_obj_move_foreground(view_context.confirmation_mask);
        lv_obj_move_foreground(view_context.confirmation_overlay);
    }
}

static void apply_pixel_shift(const pomodoro_application_view_t *view)
{
    lv_obj_t *objects[] = {
        view_context.title,
        view_context.ring,
        view_context.time,
        view_context.status,
        view_context.primary_button,
        view_context.secondary_button,
        view_context.completion_group,
        view_context.set_label,
        view_context.boundary_feedback,
        view_context.menu_button,
        view_context.top_power_status,
        view_context.page_panel,
        view_context.status_overlay,
    };
    for (size_t index = 0; index < sizeof(objects) / sizeof(objects[0]);
         index++) {
        lv_obj_set_style_translate_x(objects[index], view->pixel_shift_x, 0);
        lv_obj_set_style_translate_y(objects[index], view->pixel_shift_y, 0);
    }
    for (uint8_t index = 0; index < 4U; index++) {
        lv_obj_set_style_translate_x(view_context.set_markers[index],
                                     view->pixel_shift_x, 0);
        lv_obj_set_style_translate_y(view_context.set_markers[index],
                                     view->pixel_shift_y, 0);
    }
    for (uint8_t index = 0; index < 3U; index++) {
        lv_obj_set_style_translate_x(view_context.mode_dots[index],
                                     view->pixel_shift_x, 0);
        lv_obj_set_style_translate_y(view_context.mode_dots[index],
                                     view->pixel_shift_y, 0);
    }
}

bool pomodoro_device_view_render(const pomodoro_application_view_t *view)
{
    if (view == NULL || view_context.time == NULL) {
        return false;
    }
    view_context.last_view = *view;
    render_status_overlay(view);
    apply_pixel_shift(view);
    if (view->phase == POMODORO_APPLICATION_PHASE_PLATFORM_ERROR) {
        lv_obj_add_flag(view_context.page_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(view_context.status, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(view_context.status, application_error_text(view->error));
        lv_obj_add_flag(view_context.primary_button, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(view_context.secondary_button, LV_OBJ_FLAG_HIDDEN);
        lv_arc_set_value(view_context.ring, 0);
        lv_label_set_text(view_context.time, "--:--");
        ESP_LOGE(TAG, "Application platform error: %d", view->error);
        return true;
    }
    if (view->phase == POMODORO_APPLICATION_PHASE_TIME_SETUP ||
        view->page != POMODORO_APPLICATION_PAGE_MAIN) {
        render_auxiliary_page(view);
        return true;
    }
    lv_obj_add_flag(view_context.page_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(view_context.menu_button, LV_OBJ_FLAG_HIDDEN);

    mode_presentation_t presentation = mode_presentation(view->mode);
    lv_color_t accent = lv_color_hex(presentation.accent);
    lv_label_set_text(view_context.title, presentation.name);
    lv_obj_set_style_arc_color(view_context.ring, accent, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(view_context.primary_button, accent, 0);
    render_progress(view, accent);
    if (view->mode_boundary_feedback) {
        lv_obj_remove_flag(view_context.boundary_feedback, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(view_context.boundary_feedback, LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_add_flag(view_context.completion_group, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(view_context.ring, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(view_context.time, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(view_context.status, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(view_context.secondary_button, LV_OBJ_FLAG_HIDDEN);
    if (view->timer_state == POMODORO_TIMER_COMPLETION) {
        render_completion(view);
    } else {
        uint32_t minutes = view->remaining_seconds / 60U;
        uint32_t seconds = view->remaining_seconds % 60U;
        lv_label_set_text_fmt(view_context.time, "%02" PRIu32 ":%02" PRIu32,
                              minutes, seconds);
        uint32_t duration = view->configured_duration_seconds;
        lv_arc_set_value(view_context.ring, duration == 0U ? 0 :
            (int32_t) ((uint64_t) view->remaining_seconds * 1000U / duration));
        lv_label_set_text(view_context.primary_label,
                          view->timer_state == POMODORO_TIMER_RUNNING ? "Pause" :
                          view->timer_state == POMODORO_TIMER_PAUSED ? "Resume" :
                          "Start");
        lv_obj_remove_flag(view_context.primary_button, LV_OBJ_FLAG_HIDDEN);
        if (view->timer_state == POMODORO_TIMER_PAUSED) {
            lv_obj_add_flag(view_context.time, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_size(view_context.primary_button, 160, 48);
            lv_obj_align(view_context.primary_button, LV_ALIGN_CENTER, 0,
                         TIMER_RING_CENTER_Y - 29);
            lv_label_set_text(view_context.secondary_label, "Reset");
            lv_obj_set_size(view_context.secondary_button, 120, 40);
            lv_obj_align(view_context.secondary_button, LV_ALIGN_CENTER, 0,
                         TIMER_RING_CENTER_Y + 33);
            lv_obj_remove_flag(view_context.secondary_button,
                               LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_set_size(view_context.primary_button, 160, 48);
            lv_obj_align(view_context.primary_button, LV_ALIGN_CENTER, 0, 48);
        }
    }

    if (view->confirmation == POMODORO_APPLICATION_CONFIRM_NONE) {
        lv_obj_add_flag(view_context.confirmation_mask, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(view_context.confirmation_overlay, LV_OBJ_FLAG_HIDDEN);
    } else {
        bool reset = view->confirmation == POMODORO_APPLICATION_CONFIRM_RESET;
        lv_label_set_text(view_context.confirmation_title,
                          reset ? "Reset this timer?" : "End this timer?");
        lv_label_set_text(view_context.confirmation_button_label,
                          reset ? "Reset" : "End and switch");
        lv_obj_remove_flag(view_context.confirmation_mask,
                           LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(view_context.confirmation_overlay,
                           LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(view_context.confirmation_mask);
        lv_obj_move_foreground(view_context.confirmation_overlay);
    }
    return true;
}

esp_err_t pomodoro_device_view_start_ticks(void)
{
    if (!lvgl_port_lock(1000)) {
        return ESP_ERR_TIMEOUT;
    }
    view_context.tick_timer = lv_timer_create(tick, 1000, NULL);
    lvgl_port_unlock();
    return view_context.tick_timer != NULL ? ESP_OK : ESP_ERR_NO_MEM;
}

void pomodoro_device_view_show_startup_error(const char *message,
                                             esp_err_t error)
{
    ESP_LOGE(TAG, "%s: %s", message, esp_err_to_name(error));
    if (view_context.status != NULL && lvgl_port_lock(1000)) {
        lv_obj_remove_flag(view_context.status, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(view_context.status, message);
        lv_label_set_text(view_context.time, "--:--");
        lv_obj_add_flag(view_context.primary_button, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(view_context.secondary_button, LV_OBJ_FLAG_HIDDEN);
        lv_arc_set_value(view_context.ring, 0);
        lvgl_port_unlock();
    }
}
