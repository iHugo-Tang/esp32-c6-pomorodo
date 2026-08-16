#include "pomodoro_core.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

static bool mode_is_valid(pomodoro_mode_t mode)
{
    return mode >= POMODORO_MODE_FOCUS && mode <= POMODORO_MODE_LONG_BREAK;
}

static void prepare_ready(pomodoro_model_t *model, pomodoro_mode_t mode)
{
    model->timer.mode = mode;
    model->timer.timer_state = POMODORO_TIMER_READY;
    model->timer.configured_duration_seconds = pomodoro_duration_seconds(model, mode);
    model->timer.remaining_seconds = model->timer.configured_duration_seconds;
    model->timer.deadline_epoch_seconds = 0;
    model->timer.settled = false;
    model->timer.completion_transition_deadline_epoch_seconds = 0;
    model->timer.auto_start_cancelled = false;
}

static pomodoro_mode_t next_mode(const pomodoro_model_t *model)
{
    if (model->timer.mode != POMODORO_MODE_FOCUS) {
        return POMODORO_MODE_FOCUS;
    }
    return model->timer.set_progress == 4U
        ? POMODORO_MODE_LONG_BREAK
        : POMODORO_MODE_SHORT_BREAK;
}

static bool auto_start_enabled(const pomodoro_model_t *model)
{
    return model->timer.mode == POMODORO_MODE_FOCUS
        ? model->settings.auto_start_breaks
        : model->settings.auto_start_focus;
}

static pomodoro_event_t settle(pomodoro_model_t *model,
                               uint64_t now_epoch_seconds)
{
    if (model->timer.settled) {
        return POMODORO_EVENT_NONE;
    }

    model->timer.settled = true;
    model->timer.settlement_sequence++;
    model->timer.remaining_seconds = 0;
    model->timer.timer_state = POMODORO_TIMER_COMPLETION;
    model->timer.completion_transition_deadline_epoch_seconds =
        auto_start_enabled(model) ? now_epoch_seconds + 3U : 0U;
    model->timer.auto_start_cancelled = false;
    model->timer.last_completion = (pomodoro_completion_record_t) {
        .settlement_sequence = model->timer.settlement_sequence,
        .mode = model->timer.mode,
        .configured_duration_seconds = model->timer.configured_duration_seconds,
    };

    if (model->timer.mode == POMODORO_MODE_FOCUS) {
        if (model->timer.set_progress < 4U) {
            model->timer.set_progress++;
        }
        return POMODORO_EVENT_FOCUS_COMPLETE;
    }

    if (model->timer.mode == POMODORO_MODE_LONG_BREAK) {
        model->timer.set_progress = 0;
    }
    return POMODORO_EVENT_BREAK_COMPLETE;
}

pomodoro_settings_t pomodoro_default_settings(void)
{
    return (pomodoro_settings_t) {
        .focus_minutes = 25,
        .short_break_minutes = 5,
        .long_break_minutes = 15,
        .daily_goal = 8,
        .auto_start_breaks = true,
        .auto_start_focus = false,
        .volume = 3,
        .brightness_percent = 70,
    };
}

bool pomodoro_settings_are_valid(const pomodoro_settings_t *settings)
{
    return settings != NULL &&
           settings->focus_minutes >= 5U && settings->focus_minutes <= 60U &&
           settings->short_break_minutes >= 1U &&
           settings->short_break_minutes <= 30U &&
           settings->long_break_minutes >= 5U &&
           settings->long_break_minutes <= 60U &&
           settings->daily_goal >= 1U && settings->daily_goal <= 16U &&
           settings->volume <= 5U &&
           settings->brightness_percent >= 10U &&
           settings->brightness_percent <= 100U &&
           settings->brightness_percent % 5U == 0U;
}

pomodoro_result_t pomodoro_init(pomodoro_model_t *model,
                                const pomodoro_settings_t *settings)
{
    if (model == NULL) {
        return POMODORO_ERR_INVALID_ARGUMENT;
    }

    pomodoro_settings_t resolved = settings != NULL
        ? *settings
        : pomodoro_default_settings();
    if (!pomodoro_settings_are_valid(&resolved)) {
        return POMODORO_ERR_INVALID_ARGUMENT;
    }
    memset(model, 0, sizeof(*model));
    model->settings = resolved;
    model->timer.version = POMODORO_SNAPSHOT_VERSION;
    prepare_ready(model, POMODORO_MODE_FOCUS);
    return POMODORO_OK;
}

uint32_t pomodoro_duration_seconds(const pomodoro_model_t *model,
                                   pomodoro_mode_t mode)
{
    if (model == NULL || !mode_is_valid(mode)) {
        return 0;
    }

    uint32_t minutes = 0;
    switch (mode) {
    case POMODORO_MODE_FOCUS:
        minutes = model->settings.focus_minutes;
        break;
    case POMODORO_MODE_SHORT_BREAK:
        minutes = model->settings.short_break_minutes;
        break;
    case POMODORO_MODE_LONG_BREAK:
        minutes = model->settings.long_break_minutes;
        break;
    }
    return minutes * 60U;
}

uint32_t pomodoro_remaining_seconds(const pomodoro_model_t *model,
                                    uint64_t now_epoch_seconds)
{
    if (model == NULL) {
        return 0;
    }
    if (model->timer.timer_state != POMODORO_TIMER_RUNNING) {
        return model->timer.remaining_seconds;
    }
    if (now_epoch_seconds >= model->timer.deadline_epoch_seconds) {
        return 0;
    }

    uint64_t remaining = model->timer.deadline_epoch_seconds - now_epoch_seconds;
    return remaining > UINT32_MAX ? UINT32_MAX : (uint32_t) remaining;
}

pomodoro_result_t pomodoro_start(pomodoro_model_t *model,
                                 uint64_t now_epoch_seconds)
{
    if (model == NULL) {
        return POMODORO_ERR_INVALID_ARGUMENT;
    }
    if (model->timer.timer_state != POMODORO_TIMER_READY) {
        return POMODORO_ERR_INVALID_STATE;
    }

    if (model->timer.mode == POMODORO_MODE_FOCUS &&
        model->timer.set_progress == 4U) {
        model->timer.set_progress = 0;
    }
    model->timer.configured_duration_seconds =
        pomodoro_duration_seconds(model, model->timer.mode);
    model->timer.remaining_seconds = model->timer.configured_duration_seconds;
    model->timer.deadline_epoch_seconds =
        now_epoch_seconds + model->timer.remaining_seconds;
    model->timer.timer_state = POMODORO_TIMER_RUNNING;
    model->timer.settled = false;
    return POMODORO_OK;
}

pomodoro_result_t pomodoro_pause(pomodoro_model_t *model,
                                 uint64_t now_epoch_seconds)
{
    if (model == NULL) {
        return POMODORO_ERR_INVALID_ARGUMENT;
    }
    if (model->timer.timer_state != POMODORO_TIMER_RUNNING) {
        return POMODORO_ERR_INVALID_STATE;
    }
    if (now_epoch_seconds >= model->timer.deadline_epoch_seconds) {
        (void) settle(model, now_epoch_seconds);
        return POMODORO_OK;
    }

    model->timer.remaining_seconds =
        pomodoro_remaining_seconds(model, now_epoch_seconds);
    model->timer.deadline_epoch_seconds = 0;
    model->timer.timer_state = POMODORO_TIMER_PAUSED;
    return POMODORO_OK;
}

pomodoro_result_t pomodoro_resume(pomodoro_model_t *model,
                                  uint64_t now_epoch_seconds)
{
    if (model == NULL) {
        return POMODORO_ERR_INVALID_ARGUMENT;
    }
    if (model->timer.timer_state != POMODORO_TIMER_PAUSED) {
        return POMODORO_ERR_INVALID_STATE;
    }

    model->timer.deadline_epoch_seconds =
        now_epoch_seconds + model->timer.remaining_seconds;
    model->timer.timer_state = POMODORO_TIMER_RUNNING;
    return POMODORO_OK;
}

pomodoro_result_t pomodoro_reset(pomodoro_model_t *model)
{
    if (model == NULL) {
        return POMODORO_ERR_INVALID_ARGUMENT;
    }
    if (model->timer.timer_state != POMODORO_TIMER_PAUSED) {
        return POMODORO_ERR_INVALID_STATE;
    }

    prepare_ready(model, model->timer.mode);
    return POMODORO_OK;
}

pomodoro_result_t pomodoro_request_mode(pomodoro_model_t *model,
                                        pomodoro_mode_t mode)
{
    if (model == NULL || !mode_is_valid(mode)) {
        return POMODORO_ERR_INVALID_ARGUMENT;
    }
    if (model->timer.timer_state == POMODORO_TIMER_COMPLETION) {
        return POMODORO_ERR_INVALID_STATE;
    }
    if (model->timer.timer_state == POMODORO_TIMER_RUNNING ||
        model->timer.timer_state == POMODORO_TIMER_PAUSED) {
        return POMODORO_ERR_CONFIRMATION_REQUIRED;
    }

    prepare_ready(model, mode);
    return POMODORO_OK;
}

pomodoro_result_t pomodoro_confirm_mode_switch(pomodoro_model_t *model,
                                               pomodoro_mode_t mode)
{
    if (model == NULL || !mode_is_valid(mode)) {
        return POMODORO_ERR_INVALID_ARGUMENT;
    }
    if (model->timer.timer_state != POMODORO_TIMER_RUNNING &&
        model->timer.timer_state != POMODORO_TIMER_PAUSED) {
        return POMODORO_ERR_INVALID_STATE;
    }

    prepare_ready(model, mode);
    return POMODORO_OK;
}

pomodoro_event_t pomodoro_tick(pomodoro_model_t *model,
                               uint64_t now_epoch_seconds)
{
    if (model == NULL || model->timer.timer_state != POMODORO_TIMER_RUNNING) {
        return POMODORO_EVENT_NONE;
    }
    if (now_epoch_seconds < model->timer.deadline_epoch_seconds) {
        model->timer.remaining_seconds =
            pomodoro_remaining_seconds(model, now_epoch_seconds);
        return POMODORO_EVENT_NONE;
    }
    return settle(model, now_epoch_seconds);
}

pomodoro_result_t pomodoro_finish_completion(pomodoro_model_t *model)
{
    if (model == NULL) {
        return POMODORO_ERR_INVALID_ARGUMENT;
    }
    if (model->timer.timer_state != POMODORO_TIMER_COMPLETION) {
        return POMODORO_ERR_INVALID_STATE;
    }

    prepare_ready(model, next_mode(model));
    return POMODORO_OK;
}

pomodoro_result_t pomodoro_cancel_auto_start(pomodoro_model_t *model)
{
    if (model == NULL) {
        return POMODORO_ERR_INVALID_ARGUMENT;
    }
    if (model->timer.timer_state != POMODORO_TIMER_COMPLETION ||
        model->timer.completion_transition_deadline_epoch_seconds == 0U) {
        return POMODORO_ERR_INVALID_STATE;
    }
    model->timer.auto_start_cancelled = true;
    model->timer.completion_transition_deadline_epoch_seconds = 0;
    return POMODORO_OK;
}

bool pomodoro_auto_start_is_due(const pomodoro_model_t *model,
                                uint64_t now_epoch_seconds)
{
    return model != NULL &&
           model->timer.timer_state == POMODORO_TIMER_COMPLETION &&
           !model->timer.auto_start_cancelled &&
           model->timer.completion_transition_deadline_epoch_seconds != 0U &&
           now_epoch_seconds >=
               model->timer.completion_transition_deadline_epoch_seconds;
}

pomodoro_result_t pomodoro_start_next(pomodoro_model_t *model,
                                      uint64_t now_epoch_seconds)
{
    if (model == NULL) {
        return POMODORO_ERR_INVALID_ARGUMENT;
    }
    if (model->timer.timer_state != POMODORO_TIMER_COMPLETION) {
        return POMODORO_ERR_INVALID_STATE;
    }
    prepare_ready(model, next_mode(model));
    return pomodoro_start(model, now_epoch_seconds);
}

pomodoro_result_t pomodoro_restore(pomodoro_model_t *model,
                                   const pomodoro_snapshot_t *snapshot,
                                   uint64_t now_epoch_seconds,
                                   pomodoro_event_t *event)
{
    if (model == NULL || snapshot == NULL ||
        snapshot->version != POMODORO_SNAPSHOT_VERSION ||
        !mode_is_valid(snapshot->mode) ||
        snapshot->timer_state > POMODORO_TIMER_COMPLETION ||
        snapshot->set_progress > 4U) {
        return POMODORO_ERR_INVALID_ARGUMENT;
    }

    model->timer = *snapshot;
    if (event != NULL) {
        *event = POMODORO_EVENT_NONE;
    }
    if (snapshot->timer_state == POMODORO_TIMER_RUNNING &&
        now_epoch_seconds >= snapshot->deadline_epoch_seconds) {
        pomodoro_event_t settled_event = settle(model, now_epoch_seconds);
        if (event != NULL) {
            *event = settled_event;
        }
    }
    return POMODORO_OK;
}
