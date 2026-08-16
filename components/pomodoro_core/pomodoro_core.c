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
}

static pomodoro_event_t settle(pomodoro_model_t *model)
{
    if (model->timer.settled) {
        return POMODORO_EVENT_NONE;
    }

    model->timer.settled = true;
    model->timer.settlement_sequence++;
    model->timer.remaining_seconds = 0;
    model->timer.timer_state = POMODORO_TIMER_COMPLETION;

    if (model->timer.mode == POMODORO_MODE_FOCUS) {
        if (model->timer.set_progress < 4U) {
            model->timer.set_progress++;
        }
        model->today_completed_pomodoros++;
        model->today_completed_focus_seconds +=
            model->timer.configured_duration_seconds;
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

void pomodoro_init(pomodoro_model_t *model,
                   const pomodoro_settings_t *settings)
{
    if (model == NULL) {
        return;
    }

    memset(model, 0, sizeof(*model));
    model->settings = settings != NULL ? *settings : pomodoro_default_settings();
    model->timer.version = POMODORO_SNAPSHOT_VERSION;
    prepare_ready(model, POMODORO_MODE_FOCUS);
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
    return settle(model);
}

pomodoro_result_t pomodoro_finish_completion(pomodoro_model_t *model)
{
    if (model == NULL) {
        return POMODORO_ERR_INVALID_ARGUMENT;
    }
    if (model->timer.timer_state != POMODORO_TIMER_COMPLETION) {
        return POMODORO_ERR_INVALID_STATE;
    }

    pomodoro_mode_t next = POMODORO_MODE_FOCUS;
    if (model->timer.mode == POMODORO_MODE_FOCUS) {
        next = model->timer.set_progress == 4U
            ? POMODORO_MODE_LONG_BREAK
            : POMODORO_MODE_SHORT_BREAK;
    }
    prepare_ready(model, next);
    return POMODORO_OK;
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
        pomodoro_event_t settled_event = settle(model);
        if (event != NULL) {
            *event = settled_event;
        }
    }
    return POMODORO_OK;
}

