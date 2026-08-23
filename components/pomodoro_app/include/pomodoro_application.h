#ifndef POMODORO_APPLICATION_H
#define POMODORO_APPLICATION_H

#include <stdbool.h>
#include <stdint.h>

#include "pomodoro_agent_status.h"
#include "pomodoro_oc1_protocol.h"
#include "pomodoro_persistence.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    POMODORO_APPLICATION_OK = 0,
    POMODORO_APPLICATION_ERR_INVALID_ARGUMENT,
    POMODORO_APPLICATION_ERR_INVALID_STATE,
    POMODORO_APPLICATION_ERR_PLATFORM,
    POMODORO_APPLICATION_ERR_OUT_OF_MEMORY,
} pomodoro_application_result_t;

typedef enum {
    POMODORO_APPLICATION_ACTION_BOOT = 0,
    POMODORO_APPLICATION_ACTION_START,
    POMODORO_APPLICATION_ACTION_TICK,
    POMODORO_APPLICATION_ACTION_PAUSE,
    POMODORO_APPLICATION_ACTION_RESUME,
    POMODORO_APPLICATION_ACTION_REQUEST_RESET,
    POMODORO_APPLICATION_ACTION_CONFIRM_RESET,
    POMODORO_APPLICATION_ACTION_CANCEL_CONFIRMATION,
    POMODORO_APPLICATION_ACTION_MODE_PREVIOUS,
    POMODORO_APPLICATION_ACTION_MODE_NEXT,
    POMODORO_APPLICATION_ACTION_CONFIRM_MODE_SWITCH,
    POMODORO_APPLICATION_ACTION_START_NEXT,
    POMODORO_APPLICATION_ACTION_CANCEL_AUTO_START,
    POMODORO_APPLICATION_ACTION_NOT_NOW,
    POMODORO_APPLICATION_ACTION_OPEN_MENU,
    POMODORO_APPLICATION_ACTION_OPEN_STATISTICS,
    POMODORO_APPLICATION_ACTION_OPEN_SETTINGS,
    POMODORO_APPLICATION_ACTION_REQUEST_CLEAR_SET_PROGRESS,
    POMODORO_APPLICATION_ACTION_CONFIRM_CLEAR_SET_PROGRESS,
    POMODORO_APPLICATION_ACTION_BACK,
    POMODORO_APPLICATION_ACTION_VOLUME_UP,
    POMODORO_APPLICATION_ACTION_VOLUME_DOWN,
    POMODORO_APPLICATION_ACTION_TOGGLE_SOUND,
    POMODORO_APPLICATION_ACTION_TOGGLE_DISPLAY,
    POMODORO_APPLICATION_ACTION_POWER_OFF,
    POMODORO_APPLICATION_ACTION_OPEN_FOCUS_MINUTES,
    POMODORO_APPLICATION_ACTION_OPEN_SHORT_BREAK_MINUTES,
    POMODORO_APPLICATION_ACTION_OPEN_LONG_BREAK_MINUTES,
    POMODORO_APPLICATION_ACTION_OPEN_DAILY_GOAL,
    POMODORO_APPLICATION_ACTION_OPEN_POMODORO_COMPLETION_TRANSITION,
    POMODORO_APPLICATION_ACTION_OPEN_BRIGHTNESS,
    POMODORO_APPLICATION_ACTION_OPEN_IDLE_BRIGHTNESS,
    POMODORO_APPLICATION_ACTION_NUMERIC_DECREMENT,
    POMODORO_APPLICATION_ACTION_NUMERIC_INCREMENT,
    POMODORO_APPLICATION_ACTION_NUMERIC_CONFIRM,
    POMODORO_APPLICATION_ACTION_TOGGLE_AUTO_FOCUS,
    POMODORO_APPLICATION_ACTION_TOGGLE_AUTO_BREAK,
    POMODORO_APPLICATION_ACTION_OPEN_DATE_TIME,
    POMODORO_APPLICATION_ACTION_MANUAL_YEAR_DOWN,
    POMODORO_APPLICATION_ACTION_MANUAL_YEAR_UP,
    POMODORO_APPLICATION_ACTION_MANUAL_MONTH_DOWN,
    POMODORO_APPLICATION_ACTION_MANUAL_MONTH_UP,
    POMODORO_APPLICATION_ACTION_MANUAL_DAY_DOWN,
    POMODORO_APPLICATION_ACTION_MANUAL_DAY_UP,
    POMODORO_APPLICATION_ACTION_MANUAL_HOUR_DOWN,
    POMODORO_APPLICATION_ACTION_MANUAL_HOUR_UP,
    POMODORO_APPLICATION_ACTION_MANUAL_MINUTE_DOWN,
    POMODORO_APPLICATION_ACTION_MANUAL_MINUTE_UP,
    POMODORO_APPLICATION_ACTION_MANUAL_TIME_CONFIRM,
    POMODORO_APPLICATION_ACTION_TOGGLE_AGENT_STATUS,
    POMODORO_APPLICATION_ACTION_ACK_AGENT_STATUS,
} pomodoro_application_action_t;

typedef enum {
    POMODORO_APPLICATION_PAGE_MAIN = 0,
    POMODORO_APPLICATION_PAGE_MENU,
    POMODORO_APPLICATION_PAGE_STATISTICS,
    POMODORO_APPLICATION_PAGE_SETTINGS,
    POMODORO_APPLICATION_PAGE_NUMERIC_STEPPER,
} pomodoro_application_page_t;

typedef enum {
    POMODORO_APPLICATION_SETTING_FOCUS_MINUTES = 0,
    POMODORO_APPLICATION_SETTING_SHORT_BREAK_MINUTES,
    POMODORO_APPLICATION_SETTING_LONG_BREAK_MINUTES,
    POMODORO_APPLICATION_SETTING_DAILY_GOAL,
    POMODORO_APPLICATION_SETTING_POMODORO_COMPLETION_TRANSITION_SECONDS,
    POMODORO_APPLICATION_SETTING_BRIGHTNESS_PERCENT,
    POMODORO_APPLICATION_SETTING_IDLE_BRIGHTNESS_PERCENT,
} pomodoro_application_numeric_setting_t;

typedef struct {
    pomodoro_date_t date;
    uint16_t completed_pomodoros;
    bool is_today;
} pomodoro_application_statistics_bar_t;

typedef struct {
    uint8_t battery_percent;
    bool charging;
    bool usb_power;
    bool low_battery;
} pomodoro_application_device_status_t;

typedef struct {
    pomodoro_date_t local_date;
    uint8_t hour;
    uint8_t minute;
} pomodoro_application_manual_time_t;

typedef enum {
    POMODORO_APPLICATION_CONFIRM_NONE = 0,
    POMODORO_APPLICATION_CONFIRM_RESET,
    POMODORO_APPLICATION_CONFIRM_MODE_SWITCH,
    POMODORO_APPLICATION_CONFIRM_CLEAR_SET_PROGRESS,
} pomodoro_application_confirmation_t;

typedef enum {
    POMODORO_APPLICATION_LOAD_EMPTY = 0,
    POMODORO_APPLICATION_LOAD_LOADED,
    POMODORO_APPLICATION_LOAD_ERROR,
} pomodoro_application_load_result_t;

typedef enum {
    POMODORO_APPLICATION_PHASE_UNINITIALIZED = 0,
    POMODORO_APPLICATION_PHASE_ACTIVE,
    POMODORO_APPLICATION_PHASE_TIME_SETUP,
    POMODORO_APPLICATION_PHASE_PLATFORM_ERROR,
} pomodoro_application_phase_t;

typedef enum {
    POMODORO_APPLICATION_ERROR_NONE = 0,
    POMODORO_APPLICATION_ERROR_CLOCK,
    POMODORO_APPLICATION_ERROR_STORAGE_LOAD,
    POMODORO_APPLICATION_ERROR_STORAGE_SAVE,
    POMODORO_APPLICATION_ERROR_DISPLAY,
    POMODORO_APPLICATION_ERROR_AUDIO,
} pomodoro_application_error_t;

typedef struct {
    uint64_t wall_time_seconds;
    bool rtc_is_valid;
    pomodoro_date_t local_date;
} pomodoro_application_time_t;

typedef struct {
    pomodoro_application_phase_t phase;
    pomodoro_application_error_t error;
    pomodoro_mode_t mode;
    pomodoro_timer_state_t timer_state;
    uint32_t remaining_seconds;
    uint32_t configured_duration_seconds;
    uint8_t set_progress;
    pomodoro_settings_t settings;
    pomodoro_event_t completion_event;
    bool daily_goal_reached;
    pomodoro_application_confirmation_t confirmation;
    pomodoro_mode_t pending_mode;
    bool mode_boundary_feedback;
    pomodoro_mode_t scheduled_mode;
    bool auto_start_pending;
    uint8_t auto_start_seconds_remaining;
    bool rtc_is_valid;
    pomodoro_date_t current_local_date;
    uint8_t current_local_hour;
    uint8_t current_local_minute;
    pomodoro_application_page_t page;
    bool statistics_available;
    bool time_setup_can_cancel;
    uint16_t today_completed_pomodoros;
    uint32_t today_completed_focus_seconds;
    uint16_t today_goal_percent;
    uint8_t today_goal_fill_percent;
    uint16_t statistics_chart_scale;
    pomodoro_application_statistics_bar_t trailing_seven_days[7];
    pomodoro_application_numeric_setting_t numeric_setting;
    uint16_t numeric_draft_value;
    uint16_t numeric_minimum;
    uint16_t numeric_maximum;
    uint16_t numeric_step;
    uint8_t brightness_preview_percent;
    uint8_t last_nonzero_volume;
    bool volume_feedback_visible;
    pomodoro_application_device_status_t device_status;
    bool low_battery_warning_visible;
    bool display_awake;
    uint8_t effective_brightness_percent;
    int8_t pixel_shift_x;
    int8_t pixel_shift_y;
    pomodoro_application_manual_time_t manual_time_draft;
    pomodoro_agent_status_state_t agent_status;
    bool agent_status_visible;
    uint32_t agent_status_color;
    bool agent_status_acknowledgeable;
} pomodoro_application_view_t;

typedef struct {
    void *context;
    /* Supplies one coherent wall-clock observation for each dispatched action. */
    bool (*read_time)(void *context, pomodoro_application_time_t *time);
    /* Resolves a persisted deadline to its local Statistics Day. */
    bool (*resolve_local_date)(void *context, uint64_t wall_time_seconds,
                               pomodoro_date_t *date);
    bool (*write_time)(void *context, uint64_t wall_time_seconds);
    bool (*set_display_brightness_percent)(void *context, uint8_t percent);
    bool (*read_device_status)(void *context,
                               pomodoro_application_device_status_t *status);
    bool (*set_display_awake)(void *context, bool awake);
    bool (*power_off)(void *context);
    /* Distinguishes an unused store from an I/O failure. */
    pomodoro_application_load_result_t (*load)(
        void *context, pomodoro_persisted_state_t *state);
    /* Persists transition snapshots; TICK without completion never calls save. */
    bool (*save)(void *context, const pomodoro_persisted_state_t *state);
    /* Receives the complete read-only view after each visible state change. */
    bool (*render)(void *context, const pomodoro_application_view_t *view);
    /* One newline-terminated OC1 line, or none if no complete line is ready. */
    bool (*read_oc1_line)(void *context, char *line, size_t size,
                               bool *available);
    /* Sends one OC1 line (without a trailing newline) over the USB channel. */
    bool (*write_oc1_line)(void *context, const char *line);
    bool (*play_focus_complete)(void *context, uint8_t volume);
    bool (*play_break_complete)(void *context, uint8_t volume);
} pomodoro_application_platform_t;

typedef struct pomodoro_application pomodoro_application_t;

/*
 * Dispatch is the primary application behavior interface. Platform adapters
 * provide observations and execute effects. Manual-time rollers may submit a
 * batch adjustment, touch handling reports whether wake consumed the contact,
 * and destroy releases the state allocated by create.
 */
pomodoro_application_result_t pomodoro_application_create(
    pomodoro_application_t **application,
    const pomodoro_application_platform_t *platform);
pomodoro_application_result_t pomodoro_application_dispatch(
    pomodoro_application_t *application,
    pomodoro_application_action_t action);
pomodoro_application_result_t pomodoro_application_adjust_manual_time(
    pomodoro_application_t *application,
    pomodoro_application_action_t action, uint16_t steps);
pomodoro_application_result_t pomodoro_application_handle_touch(
    pomodoro_application_t *application, bool *consumed);
void pomodoro_application_destroy(pomodoro_application_t *application);

#ifdef __cplusplus
}
#endif

#endif
