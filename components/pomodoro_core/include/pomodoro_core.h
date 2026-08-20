#ifndef POMODORO_CORE_H
#define POMODORO_CORE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define POMODORO_SNAPSHOT_VERSION 1U
#define POMODORO_SETTINGS_VERSION 5U

typedef enum {
    POMODORO_MODE_FOCUS = 0,
    POMODORO_MODE_SHORT_BREAK,
    POMODORO_MODE_LONG_BREAK,
} pomodoro_mode_t;

typedef enum {
    POMODORO_TIMER_READY = 0,
    POMODORO_TIMER_RUNNING,
    POMODORO_TIMER_PAUSED,
    POMODORO_TIMER_COMPLETION,
} pomodoro_timer_state_t;

typedef enum {
    POMODORO_OK = 0,
    POMODORO_ERR_INVALID_ARGUMENT,
    POMODORO_ERR_INVALID_STATE,
    POMODORO_ERR_CONFIRMATION_REQUIRED,
} pomodoro_result_t;

typedef enum {
    POMODORO_EVENT_NONE = 0,
    POMODORO_EVENT_FOCUS_COMPLETE,
    POMODORO_EVENT_BREAK_COMPLETE,
} pomodoro_event_t;

typedef struct {
    uint16_t focus_minutes;
    uint16_t short_break_minutes;
    uint16_t long_break_minutes;
    uint8_t daily_goal;
    bool auto_start_breaks;
    bool auto_start_focus;
    uint8_t volume;
    uint8_t brightness_percent;
    uint8_t idle_brightness_percent;
    int8_t utc_offset_quarter_hours;
} pomodoro_settings_t;

typedef struct {
    uint32_t settlement_sequence;
    pomodoro_mode_t mode;
    uint32_t configured_duration_seconds;
} pomodoro_completion_record_t;

typedef struct {
    uint32_t version;
    pomodoro_settings_t value;
    uint8_t last_nonzero_volume;
} pomodoro_persisted_settings_t;

typedef struct {
    uint32_t version;
    pomodoro_mode_t mode;
    pomodoro_timer_state_t timer_state;
    uint32_t configured_duration_seconds;
    uint32_t remaining_seconds;
    uint64_t deadline_epoch_seconds;
    uint32_t settlement_sequence;
    bool settled;
    uint8_t set_progress;
    uint64_t completion_transition_deadline_epoch_seconds;
    bool auto_start_cancelled;
    pomodoro_completion_record_t last_completion;
} pomodoro_snapshot_t;

typedef struct {
    pomodoro_settings_t settings;
    uint8_t last_nonzero_volume;
    pomodoro_snapshot_t timer;
} pomodoro_model_t;

pomodoro_settings_t pomodoro_default_settings(void);
bool pomodoro_settings_are_valid(const pomodoro_settings_t *settings);
pomodoro_result_t pomodoro_init(pomodoro_model_t *model,
                                const pomodoro_settings_t *settings);
uint32_t pomodoro_duration_seconds(const pomodoro_model_t *model,
                                   pomodoro_mode_t mode);
pomodoro_mode_t pomodoro_scheduled_next_mode(const pomodoro_model_t *model);
uint32_t pomodoro_remaining_seconds(const pomodoro_model_t *model,
                                    uint64_t now_epoch_seconds);

pomodoro_result_t pomodoro_start(pomodoro_model_t *model,
                                 uint64_t now_epoch_seconds);
pomodoro_result_t pomodoro_pause(pomodoro_model_t *model,
                                 uint64_t now_epoch_seconds,
                                 pomodoro_event_t *event);
pomodoro_result_t pomodoro_resume(pomodoro_model_t *model,
                                  uint64_t now_epoch_seconds);
pomodoro_result_t pomodoro_reset(pomodoro_model_t *model);
pomodoro_result_t pomodoro_clear_set_progress(pomodoro_model_t *model);
pomodoro_result_t pomodoro_request_mode(pomodoro_model_t *model,
                                        pomodoro_mode_t mode);
pomodoro_result_t pomodoro_confirm_mode_switch(pomodoro_model_t *model,
                                               pomodoro_mode_t mode);
pomodoro_event_t pomodoro_tick(pomodoro_model_t *model,
                               uint64_t now_epoch_seconds);
pomodoro_result_t pomodoro_finish_completion(pomodoro_model_t *model);
pomodoro_result_t pomodoro_cancel_auto_start(pomodoro_model_t *model);
bool pomodoro_auto_start_is_due(const pomodoro_model_t *model,
                                uint64_t now_epoch_seconds);
pomodoro_result_t pomodoro_start_next(pomodoro_model_t *model,
                                      uint64_t now_epoch_seconds);
pomodoro_result_t pomodoro_restore(pomodoro_model_t *model,
                                   const pomodoro_snapshot_t *snapshot,
                                   uint64_t now_epoch_seconds,
                                   pomodoro_event_t *event);
pomodoro_result_t pomodoro_rebase_running_timer(
    pomodoro_model_t *model, uint64_t old_now_epoch_seconds,
    uint64_t new_now_epoch_seconds);
pomodoro_result_t pomodoro_rebase_running_timer_from_snapshot(
    pomodoro_model_t *model, uint64_t new_now_epoch_seconds);

#ifdef __cplusplus
}
#endif

#endif
