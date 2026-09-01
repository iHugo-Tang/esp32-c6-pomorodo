#include "pomodoro_device_view.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"

typedef struct {
    int16_t x;
    int16_t y;
    uint32_t color;
} confetti_piece_t;

static const confetti_piece_t completion_confetti[] = {
    {.x = -72, .y = 20, .color = 0xFFD166},
    {.x = -54, .y = 60, .color = 0x5B8CFF},
    {.x = -42, .y = 10, .color = 0x4DD17A},
    {.x = 44, .y = 8, .color = 0xFFD166},
    {.x = 58, .y = 57, .color = 0xFF5C5C},
    {.x = 72, .y = 26, .color = 0x5B8CFF},
};

enum {
    UI_COLOR_BACKGROUND = 0x0F1115,
    UI_COLOR_SURFACE = 0x1A1F27,
    UI_COLOR_SURFACE_RAISED = 0x232A35,
    UI_COLOR_TEXT_PRIMARY = 0xF5F7FA,
    UI_COLOR_TEXT_SECONDARY = 0xA7B0BE,
    UI_COLOR_ON_ACCENT = 0x0F1115,
    UI_COLOR_VALUE = 0x9FC2FF,
    UI_COLOR_DIVIDER = 0x303846,
    UI_COLOR_MUTED = 0x454B56,
    UI_COLOR_FOCUS = 0xFF5C5C,
    UI_COLOR_SHORT_BREAK = 0x4DD17A,
    UI_COLOR_LONG_BREAK = 0x5B8CFF,
    UI_COLOR_INTERACTIVE = UI_COLOR_LONG_BREAK,
    UI_COLOR_DANGER = 0xFF6B6B,
    UI_COLOR_WARNING = 0xFFB74D,
    UI_COLOR_CHART = 0x5B7396,
    UI_TOUCH_TARGET = 48,
    UI_BUTTON_HEIGHT = 52,
    UI_ROW_HEIGHT = 56,
    UI_RADIUS_SMALL = 12,
    UI_RADIUS_MEDIUM = 16,
    UI_RADIUS_LARGE = 24,
    UI_PAGE_INSET = 26,
    COMPLETION_CONFETTI_COUNT =
        sizeof(completion_confetti) / sizeof(completion_confetti[0]),
    TIMER_RING_CENTER_Y = -18,
    TIMER_TIME_CENTER_Y = -54,
    TIMER_PRIMARY_CENTER_Y = 19,
    COMPLETION_MANUAL_GROUP_Y = 12,
    COMPLETION_MANUAL_PRIMARY_Y = 83,
    COMPLETION_AUTO_GROUP_Y = -19,
    COMPLETION_AUTO_PRIMARY_Y = 64,
    COMPLETION_AUTO_SECONDARY_Y = 126,
    PAUSED_PRIMARY_CENTER_Y = TIMER_RING_CENTER_Y - 32,
    PAUSED_SECONDARY_CENTER_Y = TIMER_RING_CENTER_Y + 32,
    CONFIRMATION_DIALOG_MAX_HEIGHT = 250,
    CONFIRMATION_DIALOG_BODY_MAX_HEIGHT = 90,
    CONFIRMATION_DIALOG_ACTION_WIDTH = 181,
    CONFIRMATION_DIALOG_ACTION_GAP = 16,
    SETTINGS_SECTION_TEXT_COLOR = UI_COLOR_TEXT_SECONDARY,
    SETTINGS_ITEM_TEXT_COLOR = UI_COLOR_TEXT_PRIMARY,
    SETTINGS_VALUE_TEXT_COLOR = UI_COLOR_VALUE,
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
    {POMODORO_APPLICATION_ACTION_OPEN_POMODORO_COMPLETION_TRANSITION,
     "Pomodoro completion"},
    {POMODORO_APPLICATION_ACTION_TOGGLE_AUTO_BREAK, "Auto breaks"},
    {POMODORO_APPLICATION_ACTION_TOGGLE_AUTO_FOCUS, "Auto focus"},
    {POMODORO_APPLICATION_ACTION_TOGGLE_SOUND, "Sound"},
    {POMODORO_APPLICATION_ACTION_OPEN_STATISTICS, "Statistics"},
    {POMODORO_APPLICATION_ACTION_OPEN_SETTINGS, "Settings"},
    {POMODORO_APPLICATION_ACTION_REQUEST_CLEAR_SET_PROGRESS,
     "Clear set progress"},
    {POMODORO_APPLICATION_ACTION_OPEN_BRIGHTNESS, "Brightness"},
    {POMODORO_APPLICATION_ACTION_OPEN_IDLE_BRIGHTNESS, "Idle brightness"},
    {POMODORO_APPLICATION_ACTION_POWER_OFF, "Power off"},
    {POMODORO_APPLICATION_ACTION_OPEN_DATE_TIME, "Date & time"},
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
    pomodoro_device_touch_handler_t touch_handler;
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
    lv_obj_t *confirmation_body_viewport;
    lv_obj_t *confirmation_body;
    lv_obj_t *confirmation_button_label;
    lv_obj_t *mode_transition_overlay;
    lv_obj_t *menu_button;
    lv_obj_t *sound_button;
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
    lv_obj_t *settings_timer_values[5];
    lv_obj_t *settings_volume_value;
    lv_obj_t *settings_brightness_value;
    lv_obj_t *settings_idle_brightness_value;
    lv_obj_t *settings_datetime_value;
    lv_obj_t *settings_power_value;
    lv_obj_t *auto_breaks_switch;
    lv_obj_t *auto_focus_switch;
    lv_obj_t *sound_switch;
    lv_obj_t *agent_status_switch;
    lv_obj_t *waiting_sound_switch;
    lv_obj_t *agent_status_circle;
    lv_obj_t *agent_status_touch;
    lv_obj_t *auxiliary_buttons_container;
    lv_obj_t *statistics_chart;
    lv_obj_t *numeric_roller;
    lv_obj_t *manual_roller_container;
    lv_obj_t *manual_rollers[5];
    lv_obj_t *statistics_bars[7];
    lv_obj_t *statistics_dates[7];
    lv_obj_t *statistics_counts[7];
    auxiliary_button_t auxiliary_buttons[AUXILIARY_BUTTON_COUNT];
    lv_obj_t *status_overlay;
    lv_obj_t *rtc_warning;
    lv_obj_t *battery_warning;
    lv_obj_t *orientation_warning;
    lv_obj_t *volume_feedback;
    lv_obj_t *volume_icon;
    lv_obj_t *volume_segments[5];
    lv_obj_t *volume_value;
    lv_timer_t *tick_timer;
    lv_timer_t *mode_transition_timer;
    pomodoro_application_action_t pending_mode_action;
    pomodoro_application_confirmation_t rendered_confirmation;
    int32_t gesture_start_x;
    bool numeric_roller_rendering;
    bool manual_roller_rendering;
    char manual_roller_options[256];
} device_view_context_t;

typedef struct {
    const char *name;
    const char *action_name;
    uint32_t accent;
} mode_presentation_t;

static const char *TAG = "pomodoro_view";
static device_view_context_t view_context;
static lv_font_t countdown_font;

static struct {
    bool initialized;
    lv_style_t button;
    lv_style_t button_pressed;
    lv_style_t disabled;
    lv_style_t setting_row;
    lv_style_t setting_row_pressed;
    lv_style_t switch_main;
    lv_style_t switch_checked;
    lv_style_t switch_knob;
    lv_style_t switch_knob_checked;
    lv_style_t roller;
    lv_style_t roller_selected;
    lv_style_t status_badge;
} ui_styles;

static void initialize_ui_styles(void)
{
    if (ui_styles.initialized) {
        return;
    }

    lv_style_init(&ui_styles.button);
    lv_style_set_bg_color(&ui_styles.button,
                          lv_color_hex(UI_COLOR_SURFACE_RAISED));
    lv_style_set_bg_opa(&ui_styles.button, LV_OPA_COVER);
    lv_style_set_border_width(&ui_styles.button, 1);
    lv_style_set_border_color(&ui_styles.button,
                              lv_color_hex(UI_COLOR_DIVIDER));
    lv_style_set_radius(&ui_styles.button, UI_RADIUS_MEDIUM);
    lv_style_set_shadow_width(&ui_styles.button, 0);
    lv_style_set_text_color(&ui_styles.button,
                            lv_color_hex(UI_COLOR_TEXT_PRIMARY));
    lv_style_set_text_font(&ui_styles.button, &lv_font_montserrat_16);
    lv_style_set_pad_hor(&ui_styles.button, 16);

    lv_style_init(&ui_styles.button_pressed);
    lv_style_set_bg_color(&ui_styles.button_pressed,
                          lv_color_hex(UI_COLOR_SURFACE));

    lv_style_init(&ui_styles.disabled);
    lv_style_set_opa(&ui_styles.disabled, LV_OPA_50);

    lv_style_init(&ui_styles.setting_row);
    lv_style_set_bg_color(&ui_styles.setting_row,
                          lv_color_hex(UI_COLOR_SURFACE));
    lv_style_set_bg_opa(&ui_styles.setting_row, LV_OPA_COVER);
    lv_style_set_border_width(&ui_styles.setting_row, 1);
    lv_style_set_border_color(&ui_styles.setting_row,
                              lv_color_hex(UI_COLOR_DIVIDER));
    lv_style_set_radius(&ui_styles.setting_row, UI_RADIUS_MEDIUM);
    lv_style_set_pad_hor(&ui_styles.setting_row, 16);
    lv_style_set_pad_ver(&ui_styles.setting_row, 0);

    lv_style_init(&ui_styles.setting_row_pressed);
    lv_style_set_bg_color(&ui_styles.setting_row_pressed,
                          lv_color_hex(UI_COLOR_SURFACE_RAISED));

    lv_style_init(&ui_styles.switch_main);
    lv_style_set_bg_color(&ui_styles.switch_main,
                          lv_color_hex(UI_COLOR_DIVIDER));
    lv_style_set_border_width(&ui_styles.switch_main, 0);
    lv_style_set_radius(&ui_styles.switch_main, LV_RADIUS_CIRCLE);

    lv_style_init(&ui_styles.switch_checked);
    lv_style_set_bg_color(&ui_styles.switch_checked,
                          lv_color_hex(UI_COLOR_INTERACTIVE));
    lv_style_set_radius(&ui_styles.switch_checked, LV_RADIUS_CIRCLE);

    lv_style_init(&ui_styles.switch_knob);
    lv_style_set_bg_color(&ui_styles.switch_knob,
                          lv_color_hex(UI_COLOR_TEXT_SECONDARY));
    lv_style_set_radius(&ui_styles.switch_knob, LV_RADIUS_CIRCLE);

    lv_style_init(&ui_styles.switch_knob_checked);
    lv_style_set_bg_color(&ui_styles.switch_knob_checked,
                          lv_color_hex(UI_COLOR_TEXT_PRIMARY));

    lv_style_init(&ui_styles.roller);
    lv_style_set_bg_color(&ui_styles.roller,
                          lv_color_hex(UI_COLOR_SURFACE));
    lv_style_set_bg_opa(&ui_styles.roller, LV_OPA_COVER);
    lv_style_set_border_width(&ui_styles.roller, 1);
    lv_style_set_border_color(&ui_styles.roller,
                              lv_color_hex(UI_COLOR_DIVIDER));
    lv_style_set_radius(&ui_styles.roller, UI_RADIUS_MEDIUM);
    lv_style_set_text_color(&ui_styles.roller,
                            lv_color_hex(UI_COLOR_TEXT_SECONDARY));
    lv_style_set_text_font(&ui_styles.roller, &lv_font_montserrat_16);
    lv_style_set_text_line_space(&ui_styles.roller, 12);

    lv_style_init(&ui_styles.roller_selected);
    lv_style_set_bg_color(&ui_styles.roller_selected,
                          lv_color_hex(UI_COLOR_SURFACE_RAISED));
    lv_style_set_text_color(&ui_styles.roller_selected,
                            lv_color_hex(UI_COLOR_TEXT_PRIMARY));
    lv_style_set_text_font(&ui_styles.roller_selected,
                           &lv_font_montserrat_20);
    lv_style_set_border_width(&ui_styles.roller_selected, 1);
    lv_style_set_border_color(&ui_styles.roller_selected,
                              lv_color_hex(UI_COLOR_VALUE));

    lv_style_init(&ui_styles.status_badge);
    lv_style_set_bg_color(&ui_styles.status_badge,
                          lv_color_hex(UI_COLOR_SURFACE_RAISED));
    lv_style_set_bg_opa(&ui_styles.status_badge, LV_OPA_COVER);
    lv_style_set_border_width(&ui_styles.status_badge, 1);
    lv_style_set_border_color(&ui_styles.status_badge,
                              lv_color_hex(UI_COLOR_DIVIDER));
    lv_style_set_radius(&ui_styles.status_badge, UI_RADIUS_SMALL);
    lv_style_set_pad_hor(&ui_styles.status_badge, 12);
    lv_style_set_pad_ver(&ui_styles.status_badge, 8);
    lv_style_set_text_font(&ui_styles.status_badge, &lv_font_montserrat_16);

    ui_styles.initialized = true;
}

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
        view_context.action_handler(action, 1U);
    }
}

static void touch_pressed(lv_event_t *event)
{
    if (!view_context.touch_handler()) {
        return;
    }
    lv_indev_t *indev = lv_event_get_current_target(event);
    if (indev != NULL) {
        lv_indev_wait_release(indev);
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

static int16_t manual_roller_minimum(uint8_t index)
{
    static const int16_t minimums[] = {2020, 1, 1, 0, 0};
    return minimums[index % 5U];
}

static int16_t manual_roller_maximum(uint8_t index)
{
    static const int16_t maximums[] = {2069, 12, 31, 23, 59};
    return maximums[index % 5U];
}

static int16_t manual_roller_value(const pomodoro_application_view_t *view,
                                   uint8_t index)
{
    switch (index % 5U) {
    case 1:
        return view->manual_time_draft.local_date.month;
    case 2:
        return view->manual_time_draft.local_date.day;
    case 3:
        return view->manual_time_draft.hour;
    case 4:
        return view->manual_time_draft.minute;
    default:
        return view->manual_time_draft.local_date.year;
    }
}

static pomodoro_application_action_t manual_roller_action(uint8_t index,
                                                          bool up)
{
    static const pomodoro_application_action_t ups[] = {
        POMODORO_APPLICATION_ACTION_MANUAL_YEAR_UP,
        POMODORO_APPLICATION_ACTION_MANUAL_MONTH_UP,
        POMODORO_APPLICATION_ACTION_MANUAL_DAY_UP,
        POMODORO_APPLICATION_ACTION_MANUAL_HOUR_UP,
        POMODORO_APPLICATION_ACTION_MANUAL_MINUTE_UP,
    };
    static const pomodoro_application_action_t downs[] = {
        POMODORO_APPLICATION_ACTION_MANUAL_YEAR_DOWN,
        POMODORO_APPLICATION_ACTION_MANUAL_MONTH_DOWN,
        POMODORO_APPLICATION_ACTION_MANUAL_DAY_DOWN,
        POMODORO_APPLICATION_ACTION_MANUAL_HOUR_DOWN,
        POMODORO_APPLICATION_ACTION_MANUAL_MINUTE_DOWN,
    };
    return up ? ups[index % 5U] : downs[index % 5U];
}

static void manual_roller_options(uint8_t index, char *buffer, size_t size)
{
    buffer[0] = '\0';
    size_t used = 0U;
    for (int16_t value = manual_roller_minimum(index);
         value <= manual_roller_maximum(index); value++) {
        const char *format = index == 0U
            ? (value == manual_roller_maximum(index) ? "%d" : "%d\n")
            : (value == manual_roller_maximum(index) ? "%02d" : "%02d\n");
        int written = snprintf(buffer + used, size - used, format, value);
        if (written < 0 || (size_t) written >= size - used) {
            break;
        }
        used += (size_t) written;
    }
}

static void manual_roller_changed(lv_event_t *event)
{
    if (view_context.manual_roller_rendering) {
        return;
    }
    lv_obj_t *target = lv_event_get_target(event);
    for (uint8_t index = 0; index < 5U; index++) {
        if (view_context.manual_rollers[index] != target) {
            continue;
        }
        int16_t selected = manual_roller_minimum(index) +
            (int16_t) lv_roller_get_selected(target);
        int16_t current =
            manual_roller_value(&view_context.last_view, index);
        int16_t delta = selected - current;
        if (delta == 0) {
            return;
        }
        pomodoro_application_action_t action =
            manual_roller_action(index, delta > 0);
        int16_t steps = delta > 0 ? delta : -delta;
        if (view_context.action_handler != NULL) {
            view_context.action_handler(action, (uint16_t) steps);
        }
        return;
    }
}

static void render_manual_rollers(const pomodoro_application_view_t *view)
{
    view_context.manual_roller_rendering = true;
    for (uint8_t index = 0; index < 5U; index++) {
        int16_t value = manual_roller_value(view, index);
        int16_t selected = value - manual_roller_minimum(index);
        if (lv_roller_get_selected(view_context.manual_rollers[index]) !=
            (uint16_t) selected) {
            lv_roller_set_selected(view_context.manual_rollers[index],
                                   selected, LV_ANIM_OFF);
        }
        lv_obj_remove_flag(view_context.manual_rollers[index],
                           LV_OBJ_FLAG_HIDDEN);
    }
    view_context.manual_roller_rendering = false;
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
            .accent = UI_COLOR_SHORT_BREAK,
        };
    case POMODORO_MODE_LONG_BREAK:
        return (mode_presentation_t) {
            .name = "Long break",
            .action_name = "long break",
            .accent = UI_COLOR_LONG_BREAK,
        };
    default:
        return (mode_presentation_t) {
            .name = "Focus",
            .action_name = "focus",
            .accent = UI_COLOR_FOCUS,
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
    pomodoro_application_action_t action =
        POMODORO_APPLICATION_ACTION_CONFIRM_MODE_SWITCH;
    if (view_context.last_view.confirmation ==
        POMODORO_APPLICATION_CONFIRM_RESET) {
        action = POMODORO_APPLICATION_ACTION_CONFIRM_RESET;
    } else if (view_context.last_view.confirmation ==
               POMODORO_APPLICATION_CONFIRM_CLEAR_SET_PROGRESS) {
        action = POMODORO_APPLICATION_ACTION_CONFIRM_CLEAR_SET_PROGRESS;
    }
    dispatch(action);
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
    lv_dir_t direction = lv_indev_get_gesture_dir(indev);
    if (view_context.last_view.confirmation !=
        POMODORO_APPLICATION_CONFIRM_NONE) {
        if (direction == LV_DIR_RIGHT &&
            view_context.gesture_start_x < 32 &&
            point.x - view_context.gesture_start_x >= 72) {
            dispatch(POMODORO_APPLICATION_ACTION_CANCEL_CONFIRMATION);
            lv_event_stop_bubbling(event);
        }
        return;
    }
    if (view_context.last_view.timer_state == POMODORO_TIMER_COMPLETION ||
        view_context.mode_transition_timer != NULL) {
        return;
    }
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

static void agent_status_clicked(lv_event_t *event)
{
    (void) event;
    dispatch(POMODORO_APPLICATION_ACTION_ACK_AGENT_STATUS);
}

static void hide_agent_status_indicator(void)
{
    lv_obj_add_flag(view_context.agent_status_circle, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(view_context.agent_status_touch, LV_OBJ_FLAG_HIDDEN);
}

static void render_agent_status_indicator(const pomodoro_application_view_t *view)
{
    if (!view->agent_status_visible ||
        view->confirmation != POMODORO_APPLICATION_CONFIRM_NONE) {
        hide_agent_status_indicator();
        return;
    }
    lv_obj_remove_flag(view_context.agent_status_circle, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_color(view_context.agent_status_circle,
                              lv_color_hex(view->agent_status_color), 0);
    if (view->agent_status_acknowledgeable) {
        lv_obj_remove_flag(view_context.agent_status_touch, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(view_context.agent_status_touch, LV_OBJ_FLAG_HIDDEN);
    }
}

static void tick(lv_timer_t *timer)
{
    (void) timer;
    dispatch(POMODORO_APPLICATION_ACTION_TICK);
}

static void style_button(lv_obj_t *button)
{
    lv_obj_add_style(button, &ui_styles.button, LV_PART_MAIN);
    lv_obj_add_style(button, &ui_styles.button_pressed,
                     LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_add_style(button, &ui_styles.disabled,
                     LV_PART_MAIN | LV_STATE_DISABLED);
}

static void style_setting_row(lv_obj_t *row)
{
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(row, LV_PCT(100), UI_ROW_HEIGHT);
    lv_obj_add_style(row, &ui_styles.setting_row, LV_PART_MAIN);
    lv_obj_add_style(row, &ui_styles.setting_row_pressed,
                     LV_PART_MAIN | LV_STATE_PRESSED);
}

static void style_switch(lv_obj_t *toggle)
{
    lv_obj_set_size(toggle, 52, 30);
    lv_obj_set_ext_click_area(toggle, 9);
    lv_obj_add_style(toggle, &ui_styles.switch_main, LV_PART_MAIN);
    lv_obj_add_style(toggle, &ui_styles.switch_checked,
                     LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_add_style(toggle, &ui_styles.switch_knob, LV_PART_KNOB);
    lv_obj_add_style(toggle, &ui_styles.switch_knob_checked,
                     LV_PART_KNOB | LV_STATE_CHECKED);
}

static void style_roller(lv_obj_t *roller)
{
    lv_obj_add_style(roller, &ui_styles.roller, LV_PART_MAIN);
    lv_obj_add_style(roller, &ui_styles.roller_selected, LV_PART_SELECTED);
}

static void style_status_badge(lv_obj_t *label, uint32_t color)
{
    lv_obj_add_style(label, &ui_styles.status_badge, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
}

static lv_obj_t *make_text_button(lv_obj_t *parent, lv_event_cb_t callback,
                                  const char *text)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_set_size(button, 160, UI_BUTTON_HEIGHT);
    style_button(button);
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
    lv_obj_set_size(button, 160, UI_BUTTON_HEIGHT);
    style_button(button);
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
    style_setting_row(row);
    lv_obj_add_event_cb(row, action_clicked, LV_EVENT_CLICKED,
                        (void *) (uintptr_t) action);
    lv_obj_t *label = lv_label_create(row);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(
        label, lv_color_hex(SETTINGS_ITEM_TEXT_COLOR), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 0, 0);
    *toggle = lv_switch_create(row);
    style_switch(*toggle);
    lv_obj_align(*toggle, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_event_cb(*toggle, action_clicked, LV_EVENT_CLICKED,
                        (void *) (uintptr_t) action);
    return row;
}

static lv_obj_t *make_setting_action_row(
    lv_obj_t *parent, const char *text,
    pomodoro_application_action_t action, lv_obj_t **value_label)
{
    lv_obj_t *row = lv_obj_create(parent);
    style_setting_row(row);
    lv_obj_add_event_cb(row, action_clicked, LV_EVENT_CLICKED,
                        (void *) (uintptr_t) action);
    lv_obj_t *label = lv_label_create(row);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(
        label, lv_color_hex(SETTINGS_ITEM_TEXT_COLOR), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
    lv_obj_align(label, LV_ALIGN_LEFT_MID, 0, 0);
    *value_label = lv_label_create(row);
    lv_obj_set_style_text_color(
        *value_label, lv_color_hex(SETTINGS_VALUE_TEXT_COLOR), 0);
    lv_obj_set_style_text_font(*value_label, &lv_font_montserrat_16, 0);
    lv_obj_align(*value_label, LV_ALIGN_RIGHT_MID, 0, 0);
    if (action == POMODORO_APPLICATION_ACTION_POWER_OFF) {
        lv_obj_set_style_text_color(label, lv_color_hex(UI_COLOR_DANGER), 0);
        lv_obj_set_style_border_color(row, lv_color_hex(UI_COLOR_DANGER), 0);
    }
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
    lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_margin_left(label, 4, 0);
    lv_obj_set_style_margin_bottom(label, 2, 0);
    if (separated) {
        lv_obj_set_style_margin_top(label, 16, 0);
    }
    return label;
}

static void create_completion_visuals(lv_obj_t *parent)
{
    view_context.tomato = lv_obj_create(parent);
    lv_obj_remove_style_all(view_context.tomato);
    lv_obj_set_size(view_context.tomato, 74, 64);
    lv_obj_set_style_bg_opa(view_context.tomato, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(view_context.tomato,
                              lv_color_hex(UI_COLOR_FOCUS), 0);
    lv_obj_set_style_radius(view_context.tomato, 32, 0);
    lv_obj_align(view_context.tomato, LV_ALIGN_TOP_MID, 0, 2);
    lv_obj_t *leaf = lv_obj_create(view_context.tomato);
    lv_obj_remove_style_all(leaf);
    lv_obj_set_size(leaf, 30, 12);
    lv_obj_set_style_bg_opa(leaf, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(leaf, lv_color_hex(UI_COLOR_SHORT_BREAK), 0);
    lv_obj_set_style_radius(leaf, 6, 0);
    lv_obj_align(leaf, LV_ALIGN_TOP_MID, 0, 0);
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
    lv_obj_set_style_bg_color(view_context.cup,
                              lv_color_hex(UI_COLOR_LONG_BREAK), 0);
    lv_obj_set_style_radius(view_context.cup, 10, 0);
    lv_obj_align(view_context.cup, LV_ALIGN_TOP_MID, -6, 20);
    view_context.cup_handle = lv_obj_create(parent);
    lv_obj_remove_style_all(view_context.cup_handle);
    lv_obj_set_size(view_context.cup_handle, 24, 30);
    lv_obj_set_style_border_width(view_context.cup_handle, 7, 0);
    lv_obj_set_style_border_color(view_context.cup_handle,
                                  lv_color_hex(UI_COLOR_LONG_BREAK), 0);
    lv_obj_set_style_bg_opa(view_context.cup_handle, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(view_context.cup_handle, 12, 0);
    lv_obj_align(view_context.cup_handle, LV_ALIGN_TOP_MID, 43, 34);
    for (uint8_t index = 0; index < 2U; index++) {
        lv_obj_t *steam = lv_obj_create(parent);
        view_context.cup_steam[index] = steam;
        lv_obj_remove_style_all(steam);
        lv_obj_set_size(steam, 5, 18);
        lv_obj_set_style_bg_opa(steam, LV_OPA_60, 0);
        lv_obj_set_style_bg_color(steam,
                                  lv_color_hex(UI_COLOR_TEXT_SECONDARY), 0);
        lv_obj_set_style_radius(steam, 3, 0);
        lv_obj_align(steam, LV_ALIGN_TOP_MID, index == 0U ? -18 : 12, 0);
    }
}

esp_err_t pomodoro_device_view_create(
    pomodoro_device_action_handler_t action_handler,
    pomodoro_device_touch_handler_t touch_handler)
{
    if (action_handler == NULL || touch_handler == NULL ||
        !lvgl_port_lock(1000)) {
        return action_handler == NULL || touch_handler == NULL
            ? ESP_ERR_INVALID_ARG
            : ESP_ERR_TIMEOUT;
    }
    memset(&view_context, 0, sizeof(view_context));
    initialize_ui_styles();
    view_context.action_handler = action_handler;
    view_context.touch_handler = touch_handler;
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(UI_COLOR_BACKGROUND), 0);
    lv_obj_set_style_text_color(screen, lv_color_hex(UI_COLOR_TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(screen, &lv_font_montserrat_16, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(screen, gesture, LV_EVENT_GESTURE, NULL);
    lv_obj_add_event_cb(screen, gesture, LV_EVENT_PRESSED, NULL);

    view_context.title = lv_label_create(screen);
    lv_obj_set_width(view_context.title, 260);
    lv_obj_set_style_text_font(view_context.title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_align(view_context.title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(view_context.title, LV_ALIGN_TOP_MID, 0, 24);
    view_context.menu_button = make_action_button(
        screen, POMODORO_APPLICATION_ACTION_OPEN_MENU, LV_SYMBOL_LIST);
    lv_obj_set_size(view_context.menu_button, UI_TOUCH_TARGET,
                    UI_TOUCH_TARGET);
    lv_obj_set_style_text_font(lv_obj_get_child(view_context.menu_button, 0),
                               &lv_font_montserrat_24, 0);
    lv_obj_align(view_context.menu_button, LV_ALIGN_TOP_LEFT, UI_PAGE_INSET,
                 UI_PAGE_INSET);
    view_context.sound_button = make_action_button(
        screen, POMODORO_APPLICATION_ACTION_TOGGLE_SOUND,
        LV_SYMBOL_VOLUME_MID);
    lv_obj_set_size(view_context.sound_button, 48, 48);
    lv_obj_set_style_bg_opa(view_context.sound_button, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(view_context.sound_button, 0, 0);
    lv_obj_set_style_shadow_width(view_context.sound_button, 0, 0);
    lv_obj_set_style_text_color(view_context.sound_button,
                                lv_color_hex(UI_COLOR_TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(lv_obj_get_child(view_context.sound_button, 0),
                               &lv_font_montserrat_28, 0);
    lv_obj_align(view_context.sound_button, LV_ALIGN_BOTTOM_RIGHT,
                 -UI_PAGE_INSET, -UI_PAGE_INSET);
    view_context.top_power_status = lv_label_create(screen);
    lv_obj_set_width(view_context.top_power_status, 120);
    lv_obj_set_style_text_align(view_context.top_power_status,
                                LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_color(view_context.top_power_status,
                                lv_color_hex(UI_COLOR_TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(view_context.top_power_status,
                               &lv_font_montserrat_16, 0);
    lv_obj_align(view_context.top_power_status, LV_ALIGN_TOP_RIGHT,
                 -UI_PAGE_INSET, 30);

    view_context.ring = lv_arc_create(screen);
    lv_obj_set_size(view_context.ring, 288, 288);
    lv_obj_align(view_context.ring, LV_ALIGN_CENTER, 0, TIMER_RING_CENTER_Y);
    lv_arc_set_rotation(view_context.ring, 270);
    lv_arc_set_bg_angles(view_context.ring, 0, 360);
    lv_arc_set_range(view_context.ring, 0, 1000);
    lv_obj_remove_style(view_context.ring, NULL, LV_PART_KNOB);
    lv_obj_remove_flag(view_context.ring, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(view_context.ring, 13, LV_PART_MAIN);
    lv_obj_set_style_arc_color(view_context.ring, lv_color_hex(UI_COLOR_DIVIDER),
                               LV_PART_MAIN);
    lv_obj_set_style_arc_width(view_context.ring, 13, LV_PART_INDICATOR);

    view_context.status = lv_label_create(screen);
    lv_obj_set_width(view_context.status, 360);
    lv_obj_set_style_text_color(view_context.status,
                                lv_color_hex(UI_COLOR_TEXT_SECONDARY), 0);
    lv_obj_set_style_text_align(view_context.status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(view_context.status, LV_ALIGN_CENTER, 0, -78);
    lv_obj_add_flag(view_context.status, LV_OBJ_FLAG_HIDDEN);
    view_context.time = lv_label_create(screen);
    countdown_font = lv_font_montserrat_48;
    countdown_font.get_glyph_dsc = countdown_get_glyph_dsc;
    countdown_font.kerning = LV_FONT_KERNING_NONE;
    lv_obj_set_style_text_font(view_context.time, &countdown_font, 0);
    lv_obj_align(view_context.time, LV_ALIGN_CENTER, 0, TIMER_TIME_CENTER_Y);

    view_context.primary_button = make_text_button(screen, primary_clicked,
                                                   "Start");
    view_context.primary_label = lv_obj_get_child(view_context.primary_button, 0);
    lv_obj_set_style_text_font(view_context.primary_label,
                               &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(view_context.primary_label,
                                lv_color_hex(UI_COLOR_ON_ACCENT), 0);
    lv_obj_set_size(view_context.primary_button, 176, UI_BUTTON_HEIGHT);
    lv_obj_set_style_border_width(view_context.primary_button, 0, 0);
    lv_obj_align(view_context.primary_button, LV_ALIGN_CENTER, 0,
                 TIMER_PRIMARY_CENTER_Y);
    view_context.secondary_button = make_text_button(screen, secondary_clicked,
                                                     "Reset");
    view_context.secondary_label = lv_obj_get_child(view_context.secondary_button, 0);
    lv_obj_set_style_bg_opa(view_context.secondary_button, LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_color(view_context.secondary_button,
                                lv_color_hex(UI_COLOR_TEXT_SECONDARY), 0);
    lv_obj_set_style_text_color(view_context.secondary_label,
                                lv_color_hex(UI_COLOR_TEXT_SECONDARY), 0);
    lv_obj_set_size(view_context.secondary_button, 140, UI_TOUCH_TARGET);
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
                                lv_color_hex(UI_COLOR_TEXT_SECONDARY), 0);
    lv_obj_set_style_text_align(view_context.completion_body,
                                LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(view_context.completion_body, LV_ALIGN_TOP_MID, 0, 118);
    view_context.completion_countdown = lv_label_create(
        view_context.completion_group);
    lv_obj_set_width(view_context.completion_countdown, LV_PCT(100));
    lv_obj_set_style_text_align(view_context.completion_countdown,
                                LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(view_context.completion_countdown,
                               &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(view_context.completion_countdown,
                                lv_color_hex(UI_COLOR_VALUE), 0);
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
    style_status_badge(view_context.boundary_feedback,
                       UI_COLOR_TEXT_SECONDARY);
    lv_obj_align(view_context.boundary_feedback, LV_ALIGN_BOTTOM_MID, 0, -76);
    lv_obj_add_flag(view_context.boundary_feedback, LV_OBJ_FLAG_HIDDEN);

    view_context.agent_status_circle = lv_obj_create(screen);
    lv_obj_remove_style_all(view_context.agent_status_circle);
    lv_obj_set_size(view_context.agent_status_circle, 28, 28);
    lv_obj_set_style_bg_opa(view_context.agent_status_circle, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(view_context.agent_status_circle,
                              lv_color_hex(UI_COLOR_MUTED), 0);
    lv_obj_set_style_radius(view_context.agent_status_circle, LV_RADIUS_CIRCLE, 0);
    lv_obj_remove_flag(view_context.agent_status_circle, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(view_context.agent_status_circle, LV_ALIGN_BOTTOM_LEFT, 36,
                 -36);
    lv_obj_add_flag(view_context.agent_status_circle, LV_OBJ_FLAG_HIDDEN);
    view_context.agent_status_touch = lv_obj_create(screen);
    lv_obj_remove_style_all(view_context.agent_status_touch);
    lv_obj_set_size(view_context.agent_status_touch, UI_TOUCH_TARGET,
                    UI_TOUCH_TARGET);
    lv_obj_set_style_bg_opa(view_context.agent_status_touch, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(view_context.agent_status_touch, 0, 0);
    lv_obj_set_style_radius(view_context.agent_status_touch, LV_RADIUS_CIRCLE, 0);
    lv_obj_remove_flag(view_context.agent_status_touch, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(view_context.agent_status_touch, agent_status_clicked,
                        LV_EVENT_CLICKED, NULL);
    lv_obj_align(view_context.agent_status_touch, LV_ALIGN_BOTTOM_LEFT,
                 UI_PAGE_INSET, -UI_PAGE_INSET);
    lv_obj_add_flag(view_context.agent_status_touch, LV_OBJ_FLAG_HIDDEN);

    view_context.page_panel = lv_obj_create(screen);
    lv_obj_set_size(view_context.page_panel, 480, 480);
    lv_obj_align(view_context.page_panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(view_context.page_panel,
                              lv_color_hex(UI_COLOR_BACKGROUND), 0);
    lv_obj_set_style_bg_opa(view_context.page_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(view_context.page_panel, 0, 0);
    lv_obj_set_style_radius(view_context.page_panel, 0, 0);
    lv_obj_set_style_pad_all(view_context.page_panel, UI_PAGE_INSET, 0);
    lv_obj_set_flex_flow(view_context.page_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_remove_flag(view_context.page_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(view_context.page_panel, gesture, LV_EVENT_GESTURE,
                        NULL);
    lv_obj_add_event_cb(view_context.page_panel, gesture, LV_EVENT_PRESSED,
                        NULL);
    view_context.page_title = lv_label_create(view_context.page_panel);
    lv_obj_set_width(view_context.page_title, LV_PCT(100));
    lv_obj_set_height(view_context.page_title, UI_TOUCH_TARGET);
    lv_obj_set_style_pad_top(view_context.page_title, 8, 0);
    lv_obj_set_style_text_font(view_context.page_title,
                               &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_align(view_context.page_title,
                                LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(view_context.page_title,
                                lv_color_hex(UI_COLOR_TEXT_PRIMARY), 0);
    view_context.page_back_button = make_action_button(
        view_context.page_panel, POMODORO_APPLICATION_ACTION_BACK,
        LV_SYMBOL_LEFT);
    lv_obj_set_size(view_context.page_back_button, UI_TOUCH_TARGET,
                    UI_TOUCH_TARGET);
    lv_obj_set_style_text_font(
        lv_obj_get_child(view_context.page_back_button, 0),
        &lv_font_montserrat_24, 0);
    lv_obj_add_flag(view_context.page_back_button, LV_OBJ_FLAG_FLOATING);
    lv_obj_add_flag(view_context.page_back_button, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_align(view_context.page_back_button, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_add_flag(view_context.page_back_button, LV_OBJ_FLAG_HIDDEN);
    view_context.page_content = lv_obj_create(view_context.page_panel);
    lv_obj_set_width(view_context.page_content, LV_PCT(100));
    lv_obj_set_height(view_context.page_content, 0);
    lv_obj_set_flex_grow(view_context.page_content, 1);
    lv_obj_set_style_bg_opa(view_context.page_content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(view_context.page_content, 0, 0);
    lv_obj_set_style_pad_all(view_context.page_content, 0, 0);
    lv_obj_set_style_pad_row(view_context.page_content, 12, 0);
    lv_obj_set_flex_flow(view_context.page_content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(view_context.page_content, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(view_context.page_content, LV_OBJ_FLAG_EVENT_BUBBLE);
    view_context.page_body = lv_label_create(view_context.page_content);
    lv_obj_set_width(view_context.page_body, LV_PCT(100));
    lv_label_set_long_mode(view_context.page_body, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(view_context.page_body,
                                lv_color_hex(UI_COLOR_TEXT_SECONDARY), 0);
    lv_obj_set_style_text_font(view_context.page_body,
                               &lv_font_montserrat_16, 0);
    view_context.statistics_metrics = lv_obj_create(view_context.page_content);
    lv_obj_set_size(view_context.statistics_metrics, LV_PCT(100), 96);
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
                              lv_color_hex(UI_COLOR_SURFACE), 0);
    lv_obj_set_style_bg_color(view_context.statistics_goal_card,
                              lv_color_hex(UI_COLOR_SURFACE), 0);
    lv_obj_set_style_bg_opa(view_context.statistics_focus_card,
                            LV_OPA_COVER, 0);
    lv_obj_set_style_bg_opa(view_context.statistics_goal_card,
                            LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(view_context.statistics_focus_card, 1, 0);
    lv_obj_set_style_border_width(view_context.statistics_goal_card, 1, 0);
    lv_obj_set_style_border_color(view_context.statistics_focus_card,
                                  lv_color_hex(UI_COLOR_DIVIDER), 0);
    lv_obj_set_style_border_color(view_context.statistics_goal_card,
                                  lv_color_hex(UI_COLOR_DIVIDER), 0);
    lv_obj_set_style_pad_all(view_context.statistics_focus_card, 14, 0);
    lv_obj_set_style_pad_all(view_context.statistics_goal_card, 14, 0);
    lv_obj_set_style_radius(view_context.statistics_focus_card,
                            UI_RADIUS_MEDIUM, 0);
    lv_obj_set_style_radius(view_context.statistics_goal_card,
                            UI_RADIUS_MEDIUM, 0);
    view_context.statistics_focus_value = lv_label_create(
        view_context.statistics_focus_card);
    lv_label_set_recolor(view_context.statistics_focus_value, true);
    lv_obj_set_style_text_font(view_context.statistics_focus_value,
                               &lv_font_montserrat_16, 0);
    lv_obj_align(view_context.statistics_focus_value, LV_ALIGN_TOP_LEFT, 0, 0);
    view_context.statistics_goal_value = lv_label_create(
        view_context.statistics_goal_card);
    lv_label_set_recolor(view_context.statistics_goal_value, true);
    lv_obj_set_style_text_font(view_context.statistics_goal_value,
                               &lv_font_montserrat_16, 0);
    lv_obj_align(view_context.statistics_goal_value, LV_ALIGN_TOP_LEFT, 0, 0);
    view_context.statistics_goal_bar = lv_bar_create(
        view_context.statistics_goal_card);
    lv_obj_set_size(view_context.statistics_goal_bar, LV_PCT(100), 10);
    lv_obj_align(view_context.statistics_goal_bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_bar_set_range(view_context.statistics_goal_bar, 0, 100);
    lv_obj_set_style_bg_color(view_context.statistics_goal_bar,
                              lv_color_hex(UI_COLOR_DIVIDER), LV_PART_MAIN);
    lv_obj_set_style_bg_color(view_context.statistics_goal_bar,
                              lv_color_hex(SETTINGS_VALUE_TEXT_COLOR),
                              LV_PART_INDICATOR);
    lv_obj_set_style_radius(view_context.statistics_goal_bar,
                            LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_radius(view_context.statistics_goal_bar,
                            LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
    lv_obj_add_flag(view_context.statistics_metrics, LV_OBJ_FLAG_HIDDEN);
    view_context.settings_switches = lv_obj_create(view_context.page_content);
    lv_obj_set_size(view_context.settings_switches, LV_PCT(100),
                    LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(view_context.settings_switches, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(view_context.settings_switches, 0, 0);
    lv_obj_set_style_pad_all(view_context.settings_switches, 0, 0);
    lv_obj_set_style_pad_row(view_context.settings_switches, 6, 0);
    lv_obj_set_flex_flow(view_context.settings_switches,
                         LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_text_color(view_context.settings_switches,
                                lv_color_hex(UI_COLOR_TEXT_PRIMARY), 0);
    (void) make_settings_section_label(view_context.settings_switches,
                                       "Statistics", false);
    (void) make_setting_action_row(
        view_context.settings_switches, "Today's statistics",
        POMODORO_APPLICATION_ACTION_OPEN_STATISTICS,
        &view_context.settings_today_value);
    (void) make_settings_section_label(view_context.settings_switches,
                                       "Timer", true);
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
    (void) make_setting_action_row(
        view_context.settings_switches, "Pomodoro completion",
        POMODORO_APPLICATION_ACTION_OPEN_POMODORO_COMPLETION_TRANSITION,
        &view_context.settings_timer_values[4]);
    (void) make_settings_section_label(view_context.settings_switches,
                                       "Automation", true);
    (void) make_setting_switch_row(
        view_context.settings_switches, "Auto-start breaks",
        POMODORO_APPLICATION_ACTION_TOGGLE_AUTO_BREAK,
        &view_context.auto_breaks_switch);
    (void) make_setting_switch_row(
        view_context.settings_switches, "Auto-start focus",
        POMODORO_APPLICATION_ACTION_TOGGLE_AUTO_FOCUS,
        &view_context.auto_focus_switch);
    (void) make_settings_section_label(view_context.settings_switches,
                                       "Agent", true);
    (void) make_setting_switch_row(
        view_context.settings_switches, "Agent status",
        POMODORO_APPLICATION_ACTION_TOGGLE_AGENT_STATUS,
        &view_context.agent_status_switch);
    (void) make_setting_switch_row(
        view_context.settings_switches, "Waiting sound",
        POMODORO_APPLICATION_ACTION_TOGGLE_WAITING_SOUND,
        &view_context.waiting_sound_switch);
    (void) make_settings_section_label(view_context.settings_switches,
                                       "Sound", true);
    (void) make_setting_switch_row(
        view_context.settings_switches, "Sound",
        POMODORO_APPLICATION_ACTION_TOGGLE_SOUND,
        &view_context.sound_switch);
    view_context.settings_volume_value = lv_label_create(
        view_context.settings_switches);
    lv_obj_set_width(view_context.settings_volume_value, LV_PCT(100));
    lv_label_set_long_mode(view_context.settings_volume_value,
                           LV_LABEL_LONG_WRAP);
    lv_label_set_recolor(view_context.settings_volume_value, true);
    lv_obj_set_style_text_font(view_context.settings_volume_value,
                               &lv_font_montserrat_16, 0);
    lv_obj_set_style_bg_color(view_context.settings_volume_value,
                              lv_color_hex(UI_COLOR_SURFACE), 0);
    lv_obj_set_style_bg_opa(view_context.settings_volume_value, LV_OPA_COVER,
                            0);
    lv_obj_set_style_border_width(view_context.settings_volume_value, 1, 0);
    lv_obj_set_style_border_color(view_context.settings_volume_value,
                                  lv_color_hex(UI_COLOR_DIVIDER), 0);
    lv_obj_set_style_radius(view_context.settings_volume_value,
                            UI_RADIUS_MEDIUM, 0);
    lv_obj_set_style_pad_all(view_context.settings_volume_value, 16, 0);
    (void) make_settings_section_label(view_context.settings_switches,
                                       "Display", true);
    (void) make_setting_action_row(
        view_context.settings_switches, "Brightness",
        POMODORO_APPLICATION_ACTION_OPEN_BRIGHTNESS,
        &view_context.settings_brightness_value);
    (void) make_setting_action_row(
        view_context.settings_switches, "Idle brightness",
        POMODORO_APPLICATION_ACTION_OPEN_IDLE_BRIGHTNESS,
        &view_context.settings_idle_brightness_value);
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
        lv_obj_set_style_bg_color(view_context.statistics_bars[index],
                                  lv_color_hex(UI_COLOR_DIVIDER),
                                  LV_PART_MAIN);
        lv_obj_set_style_bg_opa(view_context.statistics_bars[index],
                                LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(view_context.statistics_bars[index], 8,
                                LV_PART_MAIN);
        lv_obj_set_style_radius(view_context.statistics_bars[index], 8,
                                LV_PART_INDICATOR);
        view_context.statistics_dates[index] = lv_label_create(
            view_context.statistics_chart);
        lv_obj_set_style_text_color(
            view_context.statistics_dates[index],
            lv_color_hex(SETTINGS_SECTION_TEXT_COLOR), 0);
        lv_obj_set_style_text_font(view_context.statistics_dates[index],
                                   &lv_font_montserrat_16, 0);
        lv_obj_align(view_context.statistics_dates[index],
                     LV_ALIGN_BOTTOM_LEFT, x - 2, -2);
        view_context.statistics_counts[index] = lv_label_create(
            view_context.statistics_chart);
        lv_obj_set_style_text_color(
            view_context.statistics_counts[index],
            lv_color_hex(SETTINGS_VALUE_TEXT_COLOR), 0);
        lv_obj_set_style_text_font(view_context.statistics_counts[index],
                                   &lv_font_montserrat_16, 0);
        lv_obj_align(view_context.statistics_counts[index],
                     LV_ALIGN_TOP_LEFT, x + 8, 0);
    }
    lv_obj_add_flag(view_context.statistics_chart, LV_OBJ_FLAG_HIDDEN);
    view_context.numeric_roller = lv_roller_create(view_context.page_content);
    lv_obj_set_size(view_context.numeric_roller, 240, 168);
    style_roller(view_context.numeric_roller);
    lv_roller_set_visible_row_count(view_context.numeric_roller, 3);
    lv_obj_add_event_cb(view_context.numeric_roller, numeric_roller_changed,
                        LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_flag(view_context.numeric_roller, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t *manual_roller_container = lv_obj_create(
        view_context.page_content);
    view_context.manual_roller_container = manual_roller_container;
    lv_obj_set_size(manual_roller_container, LV_PCT(100), 130);
    lv_obj_set_style_bg_opa(manual_roller_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(manual_roller_container, 0, 0);
    lv_obj_set_style_pad_all(manual_roller_container, 0, 0);
    lv_obj_set_style_pad_column(manual_roller_container, 2, 0);
    lv_obj_set_flex_flow(manual_roller_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(manual_roller_container, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    for (uint8_t index = 0; index < 5U; index++) {
        view_context.manual_rollers[index] = lv_roller_create(
            manual_roller_container);
        manual_roller_options(index, view_context.manual_roller_options,
                              sizeof(view_context.manual_roller_options));
        lv_roller_set_options(view_context.manual_rollers[index],
                              view_context.manual_roller_options,
                              LV_ROLLER_MODE_NORMAL);
        lv_obj_set_width(view_context.manual_rollers[index],
                         index == 0U ? 84 : 72);
        lv_obj_set_height(view_context.manual_rollers[index], 120);
        style_roller(view_context.manual_rollers[index]);
        lv_roller_set_visible_row_count(view_context.manual_rollers[index], 3);
        lv_obj_add_event_cb(view_context.manual_rollers[index],
                            manual_roller_changed, LV_EVENT_VALUE_CHANGED,
                            NULL);
        lv_obj_add_flag(view_context.manual_rollers[index],
                        LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_add_flag(manual_roller_container, LV_OBJ_FLAG_HIDDEN);
    (void) make_settings_section_label(view_context.settings_switches,
                                       "Date & time", true);
    (void) make_setting_action_row(
        view_context.settings_switches, "Date & time",
        POMODORO_APPLICATION_ACTION_OPEN_DATE_TIME,
        &view_context.settings_datetime_value);
    (void) make_settings_section_label(view_context.settings_switches,
                                       "Power", true);
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
    lv_obj_set_flex_align(view_context.auxiliary_buttons_container,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    for (uint8_t index = 0; index < AUXILIARY_BUTTON_COUNT; index++) {
        auxiliary_button_t *entry = &view_context.auxiliary_buttons[index];
        entry->action = auxiliary_button_definitions[index].action;
        entry->button = make_action_button(
            view_context.auxiliary_buttons_container, entry->action,
            auxiliary_button_definitions[index].text);
        entry->label = lv_obj_get_child(entry->button, 0);
        bool menu_destination =
            entry->action == POMODORO_APPLICATION_ACTION_OPEN_STATISTICS ||
            entry->action == POMODORO_APPLICATION_ACTION_OPEN_SETTINGS ||
            entry->action ==
                POMODORO_APPLICATION_ACTION_REQUEST_CLEAR_SET_PROGRESS;
        bool compact_adjustment =
            entry->action == POMODORO_APPLICATION_ACTION_NUMERIC_DECREMENT ||
            entry->action == POMODORO_APPLICATION_ACTION_NUMERIC_INCREMENT;
        bool primary_confirmation =
            entry->action == POMODORO_APPLICATION_ACTION_NUMERIC_CONFIRM ||
            entry->action ==
                POMODORO_APPLICATION_ACTION_MANUAL_TIME_CONFIRM ||
            entry->action == POMODORO_APPLICATION_ACTION_OPEN_DATE_TIME;
        lv_obj_set_size(entry->button,
                        menu_destination ? LV_PCT(100) :
                        compact_adjustment ? 72 :
                        primary_confirmation ? 220 : 136,
                        menu_destination ? 56 : UI_BUTTON_HEIGHT);
        if (primary_confirmation) {
            lv_obj_set_style_bg_color(
                entry->button, lv_color_hex(UI_COLOR_INTERACTIVE), 0);
            lv_obj_set_style_bg_color(
                entry->button,
                lv_color_darken(lv_color_hex(UI_COLOR_INTERACTIVE), LV_OPA_20),
                LV_STATE_PRESSED);
            lv_obj_set_style_border_width(entry->button, 0, 0);
            lv_obj_set_style_text_color(
                entry->label, lv_color_hex(UI_COLOR_ON_ACCENT), 0);
        }
        if (compact_adjustment) {
            lv_obj_set_style_text_font(entry->label,
                                       &lv_font_montserrat_24, 0);
        }
        if (menu_destination) {
            lv_obj_set_style_bg_color(entry->button,
                                      lv_color_hex(UI_COLOR_SURFACE), 0);
            lv_obj_align(entry->label, LV_ALIGN_LEFT_MID, 0, 0);
            lv_obj_set_style_text_color(entry->label,
                lv_color_hex(
                    entry->action ==
                            POMODORO_APPLICATION_ACTION_REQUEST_CLEAR_SET_PROGRESS
                        ? UI_COLOR_DANGER
                        : UI_COLOR_TEXT_PRIMARY),
                0);
            if (entry->action ==
                POMODORO_APPLICATION_ACTION_REQUEST_CLEAR_SET_PROGRESS) {
                lv_obj_set_style_border_color(
                    entry->button, lv_color_hex(UI_COLOR_DANGER), 0);
            }
        }
    }
    lv_obj_add_flag(view_context.page_panel, LV_OBJ_FLAG_HIDDEN);

    view_context.confirmation_mask = lv_obj_create(screen);
    lv_obj_set_size(view_context.confirmation_mask, 480, 480);
    lv_obj_align(view_context.confirmation_mask, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(view_context.confirmation_mask,
                              lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(view_context.confirmation_mask, LV_OPA_70, 0);
    lv_obj_set_style_border_width(view_context.confirmation_mask, 0, 0);
    lv_obj_set_style_radius(view_context.confirmation_mask, 0, 0);
    lv_obj_remove_flag(view_context.confirmation_mask, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(view_context.confirmation_mask, confirmation_cancelled,
                        LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(view_context.confirmation_mask, gesture,
                        LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(view_context.confirmation_mask, gesture,
                        LV_EVENT_GESTURE, NULL);
    lv_obj_add_flag(view_context.confirmation_mask, LV_OBJ_FLAG_HIDDEN);

    view_context.confirmation_overlay = lv_obj_create(screen);
    lv_obj_set_size(view_context.confirmation_overlay, 428, LV_SIZE_CONTENT);
    lv_obj_set_style_max_height(view_context.confirmation_overlay,
                                CONFIRMATION_DIALOG_MAX_HEIGHT, 0);
    lv_obj_align(view_context.confirmation_overlay, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(view_context.confirmation_overlay,
                              lv_color_hex(UI_COLOR_SURFACE_RAISED), 0);
    lv_obj_set_style_border_width(view_context.confirmation_overlay, 1, 0);
    lv_obj_set_style_border_color(view_context.confirmation_overlay,
                                  lv_color_hex(UI_COLOR_DIVIDER), 0);
    lv_obj_set_style_radius(view_context.confirmation_overlay,
                            UI_RADIUS_LARGE, 0);
    lv_obj_set_style_pad_hor(view_context.confirmation_overlay, 24, 0);
    lv_obj_set_style_pad_ver(view_context.confirmation_overlay, 20, 0);
    lv_obj_set_style_pad_row(view_context.confirmation_overlay, 0, 0);
    lv_obj_set_flex_flow(view_context.confirmation_overlay,
                         LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(view_context.confirmation_overlay,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(view_context.confirmation_overlay,
                       LV_OBJ_FLAG_SCROLLABLE);
    view_context.confirmation_title = lv_label_create(
        view_context.confirmation_overlay);
    lv_obj_set_width(view_context.confirmation_title, 360);
    lv_obj_set_style_text_font(view_context.confirmation_title,
                               &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(view_context.confirmation_title,
                                lv_color_hex(SETTINGS_ITEM_TEXT_COLOR), 0);
    lv_obj_set_style_text_align(view_context.confirmation_title,
                                LV_TEXT_ALIGN_CENTER, 0);
    view_context.confirmation_body_viewport = lv_obj_create(
        view_context.confirmation_overlay);
    lv_obj_remove_style_all(view_context.confirmation_body_viewport);
    lv_obj_set_size(view_context.confirmation_body_viewport, 360,
                    LV_SIZE_CONTENT);
    lv_obj_set_style_max_height(view_context.confirmation_body_viewport,
                                CONFIRMATION_DIALOG_BODY_MAX_HEIGHT, 0);
    lv_obj_set_style_margin_top(view_context.confirmation_body_viewport, 10,
                                0);
    lv_obj_set_scroll_dir(view_context.confirmation_body_viewport, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(view_context.confirmation_body_viewport,
                              LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_width(view_context.confirmation_body_viewport, 4,
                           LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_color(view_context.confirmation_body_viewport,
                              lv_color_hex(UI_COLOR_DIVIDER),
                              LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(view_context.confirmation_body_viewport,
                            LV_OPA_COVER, LV_PART_SCROLLBAR);
    lv_obj_set_style_radius(view_context.confirmation_body_viewport, 2,
                            LV_PART_SCROLLBAR);
    view_context.confirmation_body = lv_label_create(
        view_context.confirmation_body_viewport);
    lv_label_set_text(view_context.confirmation_body,
                      "Current progress will be discarded.");
    lv_obj_set_style_text_color(view_context.confirmation_body,
                                lv_color_hex(UI_COLOR_TEXT_SECONDARY), 0);
    lv_obj_set_style_text_font(view_context.confirmation_body,
                               &lv_font_montserrat_16, 0);
    lv_obj_set_width(view_context.confirmation_body, LV_PCT(100));
    lv_obj_set_style_text_align(view_context.confirmation_body,
                                LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_t *confirmation_actions = lv_obj_create(
        view_context.confirmation_overlay);
    lv_obj_remove_style_all(confirmation_actions);
    lv_obj_set_size(confirmation_actions, LV_PCT(100), UI_BUTTON_HEIGHT);
    lv_obj_set_style_margin_top(confirmation_actions, 20, 0);
    lv_obj_set_style_pad_column(confirmation_actions,
                                CONFIRMATION_DIALOG_ACTION_GAP, 0);
    lv_obj_set_flex_flow(confirmation_actions, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(confirmation_actions, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(confirmation_actions, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(confirmation_actions, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *cancel = make_text_button(confirmation_actions,
                                        confirmation_cancelled, "Cancel");
    lv_obj_set_size(cancel, CONFIRMATION_DIALOG_ACTION_WIDTH,
                    UI_BUTTON_HEIGHT);
    lv_obj_t *confirm = make_text_button(confirmation_actions,
                                         confirmation_accepted, "Confirm");
    view_context.confirmation_button_label = lv_obj_get_child(confirm, 0);
    lv_obj_set_size(confirm, CONFIRMATION_DIALOG_ACTION_WIDTH,
                    UI_BUTTON_HEIGHT);
    lv_obj_set_style_bg_color(confirm, lv_color_hex(UI_COLOR_DANGER), 0);
    lv_obj_set_style_bg_color(
        confirm, lv_color_darken(lv_color_hex(UI_COLOR_DANGER), LV_OPA_20),
        LV_STATE_PRESSED);
    lv_obj_set_style_border_width(confirm, 0, 0);
    lv_obj_set_style_text_color(view_context.confirmation_button_label,
                                lv_color_hex(UI_COLOR_ON_ACCENT), 0);
    lv_obj_add_flag(view_context.confirmation_overlay, LV_OBJ_FLAG_HIDDEN);

    view_context.mode_transition_overlay = lv_obj_create(screen);
    lv_obj_set_size(view_context.mode_transition_overlay, 480, 480);
    lv_obj_align(view_context.mode_transition_overlay, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(view_context.mode_transition_overlay,
                              lv_color_hex(UI_COLOR_BACKGROUND), 0);
    lv_obj_set_style_bg_opa(view_context.mode_transition_overlay,
                            LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(view_context.mode_transition_overlay, 0, 0);
    lv_obj_set_style_radius(view_context.mode_transition_overlay, 0, 0);
    lv_obj_remove_flag(view_context.mode_transition_overlay,
                       LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(view_context.mode_transition_overlay, LV_OBJ_FLAG_HIDDEN);

    view_context.status_overlay = lv_obj_create(screen);
    lv_obj_remove_style_all(view_context.status_overlay);
    lv_obj_set_size(view_context.status_overlay, 244, LV_SIZE_CONTENT);
    lv_obj_align(view_context.status_overlay, LV_ALIGN_TOP_MID, 0, 72);
    lv_obj_set_flex_flow(view_context.status_overlay, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(view_context.status_overlay, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(view_context.status_overlay, 8, 0);
    lv_obj_remove_flag(view_context.status_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(view_context.status_overlay, LV_OBJ_FLAG_SCROLLABLE);
    view_context.rtc_warning = lv_label_create(view_context.status_overlay);
    view_context.battery_warning = lv_label_create(view_context.status_overlay);
    view_context.orientation_warning =
        lv_label_create(view_context.status_overlay);
    style_status_badge(view_context.rtc_warning, UI_COLOR_WARNING);
    style_status_badge(view_context.battery_warning, UI_COLOR_WARNING);
    style_status_badge(view_context.orientation_warning,
                       UI_COLOR_TEXT_SECONDARY);
    view_context.volume_feedback = lv_obj_create(view_context.status_overlay);
    lv_obj_remove_flag(view_context.volume_feedback, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(view_context.volume_feedback, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(view_context.volume_feedback, LV_PCT(100),
                    UI_TOUCH_TARGET);
    lv_obj_set_style_bg_color(view_context.volume_feedback,
                              lv_color_hex(UI_COLOR_SURFACE_RAISED), 0);
    lv_obj_set_style_border_width(view_context.volume_feedback, 1, 0);
    lv_obj_set_style_border_color(view_context.volume_feedback,
                                  lv_color_hex(UI_COLOR_DIVIDER), 0);
    lv_obj_set_style_radius(view_context.volume_feedback,
                            UI_RADIUS_SMALL, 0);
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
        lv_obj_remove_flag(view_context.volume_segments[index],
                           LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_flag(view_context.volume_segments[index],
                           LV_OBJ_FLAG_SCROLLABLE);
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
    lv_obj_add_flag(view_context.orientation_warning, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(view_context.volume_feedback, LV_OBJ_FLAG_HIDDEN);

    lvgl_port_unlock();
    return ESP_OK;
}

static void render_progress(const pomodoro_application_view_t *view,
                            lv_color_t accent)
{
    lv_color_t focus_accent = lv_color_hex(UI_COLOR_FOCUS);
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
            index == (uint8_t) view->mode ? accent :
                                           lv_color_hex(UI_COLOR_MUTED), 0);
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
        lv_obj_set_size(view_context.secondary_button, 220, UI_TOUCH_TARGET);
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
        return "Short break duration";
    case POMODORO_APPLICATION_SETTING_LONG_BREAK_MINUTES:
        return "Long break duration";
    case POMODORO_APPLICATION_SETTING_DAILY_GOAL:
        return "Daily goal";
    case POMODORO_APPLICATION_SETTING_POMODORO_COMPLETION_TRANSITION_SECONDS:
        return "Pomodoro completion";
    case POMODORO_APPLICATION_SETTING_BRIGHTNESS_PERCENT:
        return "Brightness";
    case POMODORO_APPLICATION_SETTING_IDLE_BRIGHTNESS_PERCENT:
        return "Idle brightness";
    default:
        return "Focus duration";
    }
}

static void render_auxiliary_page(const pomodoro_application_view_t *view)
{
    hide_auxiliary_buttons();
    lv_obj_remove_flag(view_context.page_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(view_context.menu_button, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(view_context.page_content, LV_OBJ_FLAG_SCROLLABLE);
    if (view->phase == POMODORO_APPLICATION_PHASE_TIME_SETUP ||
        view->page != POMODORO_APPLICATION_PAGE_SETTINGS) {
        lv_obj_scroll_to_y(view_context.page_content, 0, LV_ANIM_OFF);
    }
    lv_obj_add_flag(view_context.statistics_chart, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(view_context.statistics_metrics, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(view_context.settings_switches, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(view_context.numeric_roller, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(view_context.manual_roller_container, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_recolor(view_context.page_body, false);
    lv_obj_set_style_text_font(view_context.page_body,
                               &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(view_context.page_body,
                                lv_color_hex(UI_COLOR_TEXT_SECONDARY), 0);
    lv_obj_set_style_text_align(view_context.page_body, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_margin_top(view_context.page_body, 0, 0);
    if (view->phase == POMODORO_APPLICATION_PHASE_TIME_SETUP &&
        !view->time_setup_can_cancel) {
        lv_obj_add_flag(view_context.page_back_button, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(view_context.page_back_button, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(view_context.page_back_button);
    }

    if (view->phase == POMODORO_APPLICATION_PHASE_TIME_SETUP) {
        char text[256];
        snprintf(
            text, sizeof(text),
            "#9FC2FF %04u-%02u-%02u  %02u:%02u#\n"
            "#A7B0BE Year / month / day  |  Hour / minute#",
            view->manual_time_draft.local_date.year,
            view->manual_time_draft.local_date.month,
            view->manual_time_draft.local_date.day,
            view->manual_time_draft.hour, view->manual_time_draft.minute);
        lv_label_set_text(view_context.page_title, "Set date & time");
        lv_label_set_recolor(view_context.page_body, true);
        lv_obj_set_style_text_align(view_context.page_body,
                                    LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(view_context.page_body, text);
        lv_obj_remove_flag(view_context.manual_roller_container,
                           LV_OBJ_FLAG_HIDDEN);
        render_manual_rollers(view);
        set_auxiliary_button_visible(
            POMODORO_APPLICATION_ACTION_MANUAL_TIME_CONFIRM, true);
        return;
    }
    if (view->page == POMODORO_APPLICATION_PAGE_MENU) {
        lv_label_set_text(view_context.page_title, "Menu");
        lv_label_set_text(view_context.page_body, "");
        set_auxiliary_button_visible(
            POMODORO_APPLICATION_ACTION_OPEN_STATISTICS, true);
        set_auxiliary_button_visible(
            POMODORO_APPLICATION_ACTION_OPEN_SETTINGS, true);
        set_auxiliary_button_visible(
            POMODORO_APPLICATION_ACTION_REQUEST_CLEAR_SET_PROGRESS, true);
        return;
    }
    if (view->page == POMODORO_APPLICATION_PAGE_STATISTICS) {
        char text[768];
        lv_label_set_recolor(view_context.page_body, true);
        lv_obj_set_style_text_font(view_context.page_body,
                                   &lv_font_montserrat_24, 0);
        snprintf(text, sizeof(text),
                 "#A7B0BE Today#    #9FC2FF x%u#",
                 view->today_completed_pomodoros);
        if (!view->statistics_available) {
            snprintf(text, sizeof(text),
                     "Statistics need a valid date and time.\n\n"
                     "Open date/time setup to restore the chart.");
            lv_label_set_recolor(view_context.page_body, false);
            lv_obj_set_style_text_font(view_context.page_body,
                                       &lv_font_montserrat_16, 0);
            lv_obj_set_style_text_align(view_context.page_body,
                                        LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_style_margin_top(view_context.page_body, 48, 0);
            set_auxiliary_button_visible(
                POMODORO_APPLICATION_ACTION_OPEN_DATE_TIME, true);
        } else {
            lv_label_set_text_fmt(view_context.statistics_focus_value,
                                  "#A7B0BE Focus time#\n#9FC2FF %" PRIu32
                                  " min#",
                                  view->today_completed_focus_seconds / 60U);
            lv_label_set_text_fmt(view_context.statistics_goal_value,
                                  "#A7B0BE Goal#\n#9FC2FF %u%%#",
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
                        ? SETTINGS_VALUE_TEXT_COLOR : UI_COLOR_CHART),
                    LV_PART_INDICATOR);
                lv_label_set_text_fmt(view_context.statistics_dates[index],
                                      "%u/%u", bar->date.month,
                                      bar->date.day);
                lv_obj_set_style_text_color(
                    view_context.statistics_dates[index],
                    lv_color_hex(bar->is_today ? UI_COLOR_VALUE :
                                                    UI_COLOR_TEXT_SECONDARY),
                    0);
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
    if (view->page == POMODORO_APPLICATION_PAGE_NUMERIC_STEPPER) {
        char text[80];
        bool is_percent =
            view->numeric_setting ==
                POMODORO_APPLICATION_SETTING_BRIGHTNESS_PERCENT ||
            view->numeric_setting ==
                POMODORO_APPLICATION_SETTING_IDLE_BRIGHTNESS_PERCENT;
        bool is_seconds = view->numeric_setting ==
            POMODORO_APPLICATION_SETTING_POMODORO_COMPLETION_TRANSITION_SECONDS;
        bool is_minutes = view->numeric_setting ==
                POMODORO_APPLICATION_SETTING_FOCUS_MINUTES ||
            view->numeric_setting ==
                POMODORO_APPLICATION_SETTING_SHORT_BREAK_MINUTES ||
            view->numeric_setting ==
                POMODORO_APPLICATION_SETTING_LONG_BREAK_MINUTES;
        snprintf(text, sizeof(text),
                 is_percent ? "%u%% to %u%%  |  Step %u%%" :
                 is_minutes ? "%u to %u min  |  Step %u min" :
                 is_seconds ? "%u to %u sec  |  Step %u sec" :
                              "%u to %u Pomodoros  |  Step %u",
                 view->numeric_minimum, view->numeric_maximum,
                 view->numeric_step);
        lv_label_set_text(view_context.page_title,
                          numeric_setting_name(view->numeric_setting));
        lv_obj_set_style_text_align(view_context.page_body,
                                    LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(view_context.page_body, text);
        set_auxiliary_button_text(
            POMODORO_APPLICATION_ACTION_NUMERIC_DECREMENT,
            LV_SYMBOL_DOWN);
        set_auxiliary_button_text(
            POMODORO_APPLICATION_ACTION_NUMERIC_INCREMENT,
            LV_SYMBOL_UP);
        char options[256] = "";
        size_t used = 0U;
        for (uint16_t value = view->numeric_minimum;
             value <= view->numeric_maximum; value += view->numeric_step) {
            const char *format = is_percent
                ? (value == view->numeric_maximum ? "%u%%" : "%u%%\n")
                : (value == view->numeric_maximum ? "%u" : "%u\n");
            int written = snprintf(options + used, sizeof(options) - used,
                                   format, value);
            if (written < 0 || (size_t) written >= sizeof(options) - used) {
                break;
            }
            used += (size_t) written;
        }
        view_context.numeric_roller_rendering = true;
        lv_roller_set_options(view_context.numeric_roller, options,
                              LV_ROLLER_MODE_NORMAL);
        lv_roller_set_selected(
            view_context.numeric_roller,
            (view->numeric_draft_value - view->numeric_minimum) /
                view->numeric_step,
            LV_ANIM_OFF);
        view_context.numeric_roller_rendering = false;
        lv_obj_remove_flag(view_context.numeric_roller, LV_OBJ_FLAG_HIDDEN);
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
    lv_label_set_text_fmt(view_context.settings_timer_values[4], "%u sec",
                          view->settings.pomodoro_completion_transition_seconds);
    lv_label_set_text_fmt(
        view_context.settings_volume_value,
        "#9FC2FF Volume %u/5#\n"
        "#A7B0BE Use the side buttons to adjust volume#",
        view->settings.volume);
    lv_label_set_text_fmt(view_context.settings_brightness_value, "%u%%",
                          view->settings.brightness_percent);
    lv_label_set_text_fmt(view_context.settings_idle_brightness_value, "%u%%",
                          view->settings.idle_brightness_percent);
    if (view->rtc_is_valid) {
        lv_label_set_text_fmt(
            view_context.settings_datetime_value,
            "%04u-%02u-%02u %02u:%02u",
            view->current_local_date.year, view->current_local_date.month,
            view->current_local_date.day, view->current_local_hour,
            view->current_local_minute);
    } else {
        lv_label_set_text(view_context.settings_datetime_value, "Set required");
    }
    if (view->device_status.usb_power && view->device_status.charging) {
        lv_label_set_text_fmt(view_context.settings_power_value,
                              "USB + charging  %u%%",
                              view->device_status.battery_percent);
    } else if (view->device_status.usb_power) {
        lv_label_set_text_fmt(view_context.settings_power_value,
                              "USB power  %u%%",
                              view->device_status.battery_percent);
    } else if (view->device_status.charging) {
        lv_label_set_text_fmt(view_context.settings_power_value,
                              "Charging  %u%%",
                              view->device_status.battery_percent);
    } else {
        lv_label_set_text_fmt(view_context.settings_power_value,
                              "Battery  %u%%",
                              view->device_status.battery_percent);
    }
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
    if (view->settings.agent_status_enabled) {
        lv_obj_add_state(view_context.agent_status_switch, LV_STATE_CHECKED);
    } else {
        lv_obj_remove_state(view_context.agent_status_switch, LV_STATE_CHECKED);
    }
    if (view->settings.waiting_sound_enabled) {
        lv_obj_add_state(view_context.waiting_sound_switch, LV_STATE_CHECKED);
    } else {
        lv_obj_remove_state(view_context.waiting_sound_switch,
                            LV_STATE_CHECKED);
    }
    lv_obj_add_flag(view_context.page_content, LV_OBJ_FLAG_SCROLLABLE);
}

static void render_status_overlay(const pomodoro_application_view_t *view)
{
    lv_obj_remove_flag(view_context.status_overlay, LV_OBJ_FLAG_HIDDEN);
    if (view->rtc_is_valid ||
        view->phase == POMODORO_APPLICATION_PHASE_TIME_SETUP) {
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
        view->device_status.usb_power ? LV_SYMBOL_USB " " : "",
        view->device_status.charging ? LV_SYMBOL_CHARGE " " : "",
        battery_icon, view->device_status.battery_percent);
    lv_obj_set_style_text_color(
        view_context.top_power_status,
        lv_color_hex(view->device_status.low_battery ? UI_COLOR_WARNING :
                                                       UI_COLOR_TEXT_PRIMARY),
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
                    ? SETTINGS_VALUE_TEXT_COLOR : UI_COLOR_MUTED),
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
        view_context.sound_button,
        view_context.top_power_status,
        view_context.page_panel,
        view_context.status_overlay,
        view_context.agent_status_circle,
        view_context.agent_status_touch,
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

static void render_confirmation(const pomodoro_application_view_t *view)
{
    if (view->confirmation == POMODORO_APPLICATION_CONFIRM_NONE) {
        lv_obj_add_flag(view_context.confirmation_mask, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(view_context.confirmation_overlay, LV_OBJ_FLAG_HIDDEN);
        view_context.rendered_confirmation =
            POMODORO_APPLICATION_CONFIRM_NONE;
        return;
    }

    bool confirmation_changed =
        view_context.rendered_confirmation != view->confirmation;

    const char *title = "End this timer?";
    const char *body = "Current progress will be discarded.";
    const char *button = "End and switch";
    if (view->confirmation == POMODORO_APPLICATION_CONFIRM_RESET) {
        title = "Reset this timer?";
        button = "Reset";
    } else if (view->confirmation ==
               POMODORO_APPLICATION_CONFIRM_CLEAR_SET_PROGRESS) {
        title = "Clear set progress?";
        body = "This resets the current set to 0/4. Today's statistics won't "
               "be changed.";
        button = "Clear";
    }
    lv_label_set_text(view_context.confirmation_title, title);
    lv_label_set_text(view_context.confirmation_body, body);
    lv_label_set_text(view_context.confirmation_button_label, button);
    lv_obj_remove_flag(view_context.confirmation_mask, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(view_context.confirmation_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_update_layout(view_context.confirmation_overlay);
    if (confirmation_changed) {
        lv_obj_scroll_to_y(view_context.confirmation_body_viewport, 0,
                           LV_ANIM_OFF);
    }
    view_context.rendered_confirmation = view->confirmation;
    lv_obj_align(view_context.confirmation_overlay, LV_ALIGN_CENTER, 0, 0);
    lv_obj_move_foreground(view_context.confirmation_mask);
    lv_obj_move_foreground(view_context.confirmation_overlay);
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
        lv_obj_add_flag(view_context.menu_button, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(view_context.sound_button, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(view_context.completion_group, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(view_context.confirmation_mask, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(view_context.confirmation_overlay, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(view_context.status_overlay, LV_OBJ_FLAG_HIDDEN);
        hide_agent_status_indicator();
        lv_obj_add_flag(view_context.top_power_status, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(view_context.set_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(view_context.boundary_feedback, LV_OBJ_FLAG_HIDDEN);
        for (uint8_t index = 0; index < 4U; index++) {
            lv_obj_add_flag(view_context.set_markers[index],
                            LV_OBJ_FLAG_HIDDEN);
        }
        for (uint8_t index = 0; index < 3U; index++) {
            lv_obj_add_flag(view_context.mode_dots[index], LV_OBJ_FLAG_HIDDEN);
        }
        lv_label_set_text(view_context.title, "Device error");
        lv_obj_remove_flag(view_context.status, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(view_context.status, application_error_text(view->error));
        lv_obj_align(view_context.status, LV_ALIGN_CENTER, 0, 0);
        lv_obj_add_flag(view_context.primary_button, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(view_context.secondary_button, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(view_context.ring, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(view_context.time, LV_OBJ_FLAG_HIDDEN);
        ESP_LOGE(TAG, "Application platform error: %d", view->error);
        return true;
    }
    if (view->phase == POMODORO_APPLICATION_PHASE_TIME_SETUP ||
        view->page != POMODORO_APPLICATION_PAGE_MAIN) {
        lv_obj_add_flag(view_context.sound_button, LV_OBJ_FLAG_HIDDEN);
        hide_agent_status_indicator();
        render_auxiliary_page(view);
        render_confirmation(view);
        return true;
    }
    lv_obj_add_flag(view_context.page_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(view_context.menu_button, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(view_context.sound_button, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(lv_obj_get_child(view_context.sound_button, 0),
                      view->settings.volume == 0U ? LV_SYMBOL_MUTE :
                                                    LV_SYMBOL_VOLUME_MID);
    if (view->auto_start_pending) {
        lv_obj_add_state(view_context.sound_button, LV_STATE_DISABLED);
    } else {
        lv_obj_remove_state(view_context.sound_button, LV_STATE_DISABLED);
    }

    mode_presentation_t presentation = mode_presentation(view->mode);
    lv_color_t accent = lv_color_hex(presentation.accent);
    lv_label_set_text(view_context.title, presentation.name);
    lv_obj_set_style_arc_color(view_context.ring, accent, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(view_context.primary_button, accent, 0);
    lv_obj_set_style_bg_color(
        view_context.primary_button, lv_color_darken(accent, LV_OPA_20),
        LV_STATE_PRESSED);
    lv_obj_set_style_text_color(view_context.primary_label,
                                lv_color_hex(UI_COLOR_ON_ACCENT), 0);
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
    lv_obj_set_style_text_font(view_context.secondary_label,
                               &lv_font_montserrat_16, 0);
    lv_obj_add_flag(view_context.secondary_button, LV_OBJ_FLAG_HIDDEN);
    if (view->timer_state == POMODORO_TIMER_COMPLETION) {
        lv_obj_add_flag(view_context.menu_button, LV_OBJ_FLAG_HIDDEN);
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
            lv_obj_set_size(view_context.primary_button, 176,
                            UI_BUTTON_HEIGHT);
            lv_obj_align(view_context.primary_button, LV_ALIGN_CENTER, 0,
                         PAUSED_PRIMARY_CENTER_Y);
            lv_label_set_text(view_context.secondary_label, "Reset");
            lv_obj_set_style_text_font(view_context.secondary_label,
                                       &lv_font_montserrat_20, 0);
            lv_obj_set_size(view_context.secondary_button, 176,
                            UI_BUTTON_HEIGHT);
            lv_obj_align(view_context.secondary_button, LV_ALIGN_CENTER, 0,
                         PAUSED_SECONDARY_CENTER_Y);
            lv_obj_remove_flag(view_context.secondary_button,
                               LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_set_size(view_context.primary_button, 176,
                            UI_BUTTON_HEIGHT);
            lv_obj_align(view_context.primary_button, LV_ALIGN_CENTER, 0,
                         TIMER_PRIMARY_CENTER_Y);
        }
    }

    render_agent_status_indicator(view);
    render_confirmation(view);
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

esp_err_t pomodoro_device_view_bind_touch(void)
{
    if (!lvgl_port_lock(1000)) {
        return ESP_ERR_TIMEOUT;
    }
    bool bound = false;
    for (lv_indev_t *indev = lv_indev_get_next(NULL); indev != NULL;
         indev = lv_indev_get_next(indev)) {
        if (lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER) {
            lv_indev_set_gesture_min_distance(indev, 72);
            lv_indev_add_event_cb(indev, touch_pressed, LV_EVENT_PRESSED,
                                  NULL);
            bound = true;
        }
    }
    lvgl_port_unlock();
    return bound ? ESP_OK : ESP_ERR_NOT_FOUND;
}

void pomodoro_device_view_show_startup_error(const char *message,
                                             esp_err_t error)
{
    ESP_LOGE(TAG, "%s: %s", message, esp_err_to_name(error));
    if (view_context.status != NULL && lvgl_port_lock(1000)) {
        lv_label_set_text(view_context.title, "Startup error");
        lv_obj_remove_flag(view_context.status, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(view_context.status, message);
        lv_obj_align(view_context.status, LV_ALIGN_CENTER, 0, 0);
        lv_obj_add_flag(view_context.time, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(view_context.ring, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(view_context.menu_button, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(view_context.primary_button, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(view_context.secondary_button, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(view_context.sound_button, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(view_context.page_panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(view_context.completion_group, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(view_context.confirmation_mask, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(view_context.confirmation_overlay, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(view_context.status_overlay, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(view_context.top_power_status, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(view_context.set_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(view_context.boundary_feedback, LV_OBJ_FLAG_HIDDEN);
        hide_agent_status_indicator();
        for (uint8_t index = 0; index < 4U; index++) {
            lv_obj_add_flag(view_context.set_markers[index],
                            LV_OBJ_FLAG_HIDDEN);
        }
        for (uint8_t index = 0; index < 3U; index++) {
            lv_obj_add_flag(view_context.mode_dots[index], LV_OBJ_FLAG_HIDDEN);
        }
        lv_arc_set_value(view_context.ring, 0);
        lvgl_port_unlock();
    }
}

static void hide_orientation_warning(lv_timer_t *timer)
{
    (void) timer;
    lv_obj_add_flag(view_context.orientation_warning, LV_OBJ_FLAG_HIDDEN);
}

void pomodoro_device_view_show_orientation_warning(void)
{
    if (!lvgl_port_lock(1000)) {
        return;
    }
    lv_label_set_text(view_context.orientation_warning,
                      "Auto-rotate unavailable");
    lv_obj_remove_flag(view_context.orientation_warning, LV_OBJ_FLAG_HIDDEN);
    lv_timer_t *timer =
        lv_timer_create(hide_orientation_warning, 3000, NULL);
    lv_timer_set_repeat_count(timer, 1);
    lvgl_port_unlock();
}
