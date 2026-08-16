#ifndef POMODORO_APPLICATION_H
#define POMODORO_APPLICATION_H

#include <stdbool.h>
#include <stdint.h>

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
} pomodoro_application_action_t;

typedef enum {
    POMODORO_APPLICATION_CONFIRM_NONE = 0,
    POMODORO_APPLICATION_CONFIRM_RESET,
    POMODORO_APPLICATION_CONFIRM_MODE_SWITCH,
} pomodoro_application_confirmation_t;

typedef enum {
    POMODORO_APPLICATION_LOAD_EMPTY = 0,
    POMODORO_APPLICATION_LOAD_LOADED,
    POMODORO_APPLICATION_LOAD_ERROR,
} pomodoro_application_load_result_t;

typedef enum {
    POMODORO_APPLICATION_PHASE_UNINITIALIZED = 0,
    POMODORO_APPLICATION_PHASE_ACTIVE,
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
    uint64_t epoch_seconds;
    bool rtc_is_valid;
    pomodoro_date_t local_date;
} pomodoro_application_time_t;

typedef struct {
    pomodoro_application_phase_t phase;
    pomodoro_application_error_t error;
    pomodoro_mode_t mode;
    pomodoro_timer_state_t timer_state;
    uint32_t remaining_seconds;
    uint8_t set_progress;
    pomodoro_settings_t settings;
    pomodoro_event_t completion_event;
    pomodoro_application_confirmation_t confirmation;
    pomodoro_mode_t pending_mode;
    bool mode_boundary_feedback;
    pomodoro_mode_t scheduled_mode;
    bool auto_start_pending;
    uint8_t auto_start_seconds_remaining;
} pomodoro_application_view_t;

typedef struct {
    void *context;
    /* Supplies one coherent wall-clock observation for each dispatched action. */
    bool (*read_time)(void *context, pomodoro_application_time_t *time);
    /* Distinguishes an unused store from an I/O failure. */
    pomodoro_application_load_result_t (*load)(
        void *context, pomodoro_persisted_state_t *state);
    /* Persists transition snapshots; TICK without completion never calls save. */
    bool (*save)(void *context, const pomodoro_persisted_state_t *state);
    /* Receives the complete read-only view after each visible state change. */
    bool (*render)(void *context, const pomodoro_application_view_t *view);
    bool (*play_focus_complete)(void *context, uint8_t volume);
    bool (*play_break_complete)(void *context, uint8_t volume);
} pomodoro_application_platform_t;

typedef struct pomodoro_application pomodoro_application_t;

/*
 * The application boundary is create plus dispatch. Platform adapters provide
 * observations and execute effects. The application owns all mutable state and
 * releases it with destroy.
 */
pomodoro_application_result_t pomodoro_application_create(
    pomodoro_application_t **application,
    const pomodoro_application_platform_t *platform);
pomodoro_application_result_t pomodoro_application_dispatch(
    pomodoro_application_t *application,
    pomodoro_application_action_t action);
void pomodoro_application_destroy(pomodoro_application_t *application);

#ifdef __cplusplus
}
#endif

#endif
