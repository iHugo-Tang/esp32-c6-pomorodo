#include "pomodoro_main_view.h"

#include <inttypes.h>
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
};

typedef struct {
    pomodoro_main_action_handler_t action_handler;
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
    lv_timer_t *tick_timer;
    lv_timer_t *mode_transition_timer;
    pomodoro_application_action_t pending_mode_action;
} main_view_context_t;

typedef struct {
    const char *name;
    const char *action_name;
    uint32_t accent;
} mode_presentation_t;

static const char *TAG = "pomodoro_view";
static main_view_context_t view_context;
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

static void mode_transition_finished(lv_timer_t *timer)
{
    (void) timer;
    pomodoro_application_action_t action = view_context.pending_mode_action;
    view_context.mode_transition_timer = NULL;
    dispatch(action);
    lv_obj_fade_in(lv_screen_active(), 60, 0);
}

static void gesture(lv_event_t *event)
{
    (void) event;
    lv_indev_t *indev = lv_indev_active();
    if (indev == NULL || view_context.last_view.timer_state ==
                             POMODORO_TIMER_COMPLETION ||
        view_context.last_view.confirmation !=
            POMODORO_APPLICATION_CONFIRM_NONE ||
        view_context.mode_transition_timer != NULL) {
        return;
    }
    lv_dir_t direction = lv_indev_get_gesture_dir(indev);
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
    lv_obj_fade_out(lv_screen_active(), 60, 0);
    view_context.mode_transition_timer = lv_timer_create(
        mode_transition_finished, 60, NULL);
    if (view_context.mode_transition_timer != NULL) {
        lv_timer_set_repeat_count(view_context.mode_transition_timer, 1);
    } else {
        dispatch(view_context.pending_mode_action);
        lv_obj_fade_in(lv_screen_active(), 60, 0);
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
    lv_obj_align(smile, LV_ALIGN_CENTER, 0, 13);

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

esp_err_t pomodoro_main_view_create(
    pomodoro_main_action_handler_t action_handler)
{
    if (action_handler == NULL || !lvgl_port_lock(1000)) {
        return action_handler == NULL ? ESP_ERR_INVALID_ARG : ESP_ERR_TIMEOUT;
    }
    memset(&view_context, 0, sizeof(view_context));
    view_context.action_handler = action_handler;
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x07090D), 0);
    lv_obj_set_style_text_color(screen, lv_color_hex(0xF5F7FA), 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(screen, gesture, LV_EVENT_GESTURE, NULL);

    view_context.title = lv_label_create(screen);
    lv_obj_set_style_text_font(view_context.title, &lv_font_montserrat_24, 0);
    lv_obj_align(view_context.title, LV_ALIGN_TOP_MID, 0, 24);

    view_context.ring = lv_arc_create(screen);
    lv_obj_set_size(view_context.ring, 288, 288);
    lv_obj_align(view_context.ring, LV_ALIGN_CENTER, 0, -18);
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
    lv_obj_set_size(view_context.completion_group, 360, 250);
    lv_obj_align(view_context.completion_group, LV_ALIGN_CENTER, 0, -22);
    create_completion_visuals(view_context.completion_group);
    view_context.completion_title = lv_label_create(view_context.completion_group);
    lv_obj_set_style_text_font(view_context.completion_title,
                               &lv_font_montserrat_24, 0);
    lv_obj_align(view_context.completion_title, LV_ALIGN_TOP_MID, 0, 82);
    view_context.completion_body = lv_label_create(view_context.completion_group);
    lv_obj_set_style_text_color(view_context.completion_body,
                                lv_color_hex(0xAAB1BE), 0);
    lv_obj_align(view_context.completion_body, LV_ALIGN_TOP_MID, 0, 118);
    view_context.completion_countdown = lv_label_create(
        view_context.completion_group);
    lv_obj_align(view_context.completion_countdown, LV_ALIGN_TOP_MID, 0, 150);
    lv_obj_add_flag(view_context.completion_group, LV_OBJ_FLAG_HIDDEN);

    view_context.set_label = lv_label_create(screen);
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
        lv_label_set_text(view_context.completion_body, "Great work! +1");
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
        if (focus && view->completion_event == POMODORO_EVENT_FOCUS_COMPLETE) {
            lv_obj_remove_flag(view_context.confetti[index], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(view_context.confetti[index], LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (view->auto_start_pending) {
        lv_label_set_text_fmt(view_context.completion_countdown,
                              "%s starts in %u",
                              scheduled.name,
                              view->auto_start_seconds_remaining);
        lv_label_set_text(view_context.primary_label, "Start now");
        lv_label_set_text(view_context.secondary_label, "Cancel auto-start");
    } else {
        lv_label_set_text(view_context.completion_countdown, "");
        lv_label_set_text_fmt(view_context.primary_label, "Start %s",
                              scheduled.action_name);
        lv_label_set_text(view_context.secondary_label, "Not now");
    }
    lv_obj_remove_flag(view_context.primary_button, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(view_context.secondary_button, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_size(view_context.primary_button, 220, 56);
    lv_obj_align(view_context.primary_button, LV_ALIGN_CENTER, 0, 48);
    lv_obj_set_size(view_context.secondary_button, 170, 44);
    lv_obj_align(view_context.secondary_button, LV_ALIGN_CENTER, 0, 101);
}

bool pomodoro_main_view_render(const pomodoro_application_view_t *view)
{
    if (view == NULL || view_context.time == NULL) {
        return false;
    }
    view_context.last_view = *view;
    if (view->phase == POMODORO_APPLICATION_PHASE_PLATFORM_ERROR) {
        lv_obj_remove_flag(view_context.status, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(view_context.status, application_error_text(view->error));
        lv_obj_add_flag(view_context.primary_button, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(view_context.secondary_button, LV_OBJ_FLAG_HIDDEN);
        lv_arc_set_value(view_context.ring, 0);
        lv_label_set_text(view_context.time, "--:--");
        ESP_LOGE(TAG, "Application platform error: %d", view->error);
        return true;
    }

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
            lv_obj_align(view_context.primary_button, LV_ALIGN_CENTER, 0, -29);
            lv_label_set_text(view_context.secondary_label, "Reset");
            lv_obj_set_size(view_context.secondary_button, 120, 40);
            lv_obj_align(view_context.secondary_button, LV_ALIGN_CENTER, 0, 33);
            lv_obj_remove_flag(view_context.secondary_button,
                               LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_set_size(view_context.primary_button, 220, 56);
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

esp_err_t pomodoro_main_view_start_ticks(void)
{
    if (!lvgl_port_lock(1000)) {
        return ESP_ERR_TIMEOUT;
    }
    view_context.tick_timer = lv_timer_create(tick, 1000, NULL);
    lvgl_port_unlock();
    return view_context.tick_timer != NULL ? ESP_OK : ESP_ERR_NO_MEM;
}

void pomodoro_main_view_show_startup_error(const char *message,
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
