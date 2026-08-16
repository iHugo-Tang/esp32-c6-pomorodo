#ifndef POMODORO_CORE_H
#define POMODORO_CORE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define POMODORO_SNAPSHOT_VERSION 1U

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
} pomodoro_settings_t;

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
} pomodoro_snapshot_t;

typedef struct {
    pomodoro_settings_t settings;
    pomodoro_snapshot_t timer;
    uint16_t today_completed_pomodoros;
    uint32_t today_completed_focus_seconds;
    bool daily_goal_celebrated;
} pomodoro_model_t;

pomodoro_settings_t pomodoro_default_settings(void);
void pomodoro_init(pomodoro_model_t *model,
                   const pomodoro_settings_t *settings);
uint32_t pomodoro_duration_seconds(const pomodoro_model_t *model,
                                   pomodoro_mode_t mode);
uint32_t pomodoro_remaining_seconds(const pomodoro_model_t *model,
                                    uint64_t now_epoch_seconds);

pomodoro_result_t pomodoro_start(pomodoro_model_t *model,
                                 uint64_t now_epoch_seconds);
pomodoro_result_t pomodoro_pause(pomodoro_model_t *model,
                                 uint64_t now_epoch_seconds);
pomodoro_result_t pomodoro_resume(pomodoro_model_t *model,
                                  uint64_t now_epoch_seconds);
pomodoro_result_t pomodoro_reset(pomodoro_model_t *model);
pomodoro_result_t pomodoro_request_mode(pomodoro_model_t *model,
                                        pomodoro_mode_t mode);
pomodoro_result_t pomodoro_confirm_mode_switch(pomodoro_model_t *model,
                                               pomodoro_mode_t mode);
pomodoro_event_t pomodoro_tick(pomodoro_model_t *model,
                               uint64_t now_epoch_seconds);
pomodoro_result_t pomodoro_finish_completion(pomodoro_model_t *model);
pomodoro_result_t pomodoro_restore(pomodoro_model_t *model,
                                   const pomodoro_snapshot_t *snapshot,
                                   uint64_t now_epoch_seconds,
                                   pomodoro_event_t *event);

#ifdef __cplusplus
}
#endif

#endif

