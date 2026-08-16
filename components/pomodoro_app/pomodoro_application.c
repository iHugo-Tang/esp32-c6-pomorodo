#include "pomodoro_application.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

struct pomodoro_application {
    pomodoro_application_platform_t platform;
    pomodoro_model_t model;
    pomodoro_statistics_store_t statistics;
    pomodoro_application_view_t view;
    bool daily_goal_reached_for_completion;
    bool booted;
};

static uint16_t completed_pomodoros_on(
    const pomodoro_statistics_store_t *statistics, pomodoro_date_t date)
{
    const pomodoro_daily_statistics_t *day =
        pomodoro_statistics_find(statistics, date);
    return day != NULL ? day->completed_pomodoros : 0U;
}

static bool platform_is_valid(const pomodoro_application_platform_t *platform)
{
    return platform != NULL && platform->read_time != NULL &&
           platform->load != NULL && platform->save != NULL &&
           platform->render != NULL &&
           platform->play_focus_complete != NULL &&
           platform->play_break_complete != NULL;
}

static void snapshot_view(pomodoro_application_t *application,
                          const pomodoro_application_time_t *time,
                          pomodoro_event_t completion_event)
{
    application->view.mode = application->model.timer.mode;
    application->view.timer_state = application->model.timer.timer_state;
    application->view.configured_duration_seconds =
        application->model.timer.configured_duration_seconds;
    application->view.remaining_seconds = time != NULL
        ? pomodoro_remaining_seconds(&application->model, time->epoch_seconds)
        : application->model.timer.remaining_seconds;
    application->view.set_progress = application->model.timer.set_progress;
    application->view.settings = application->model.settings;
    application->view.completion_event = completion_event;
    if (application->model.timer.timer_state != POMODORO_TIMER_COMPLETION) {
        application->daily_goal_reached_for_completion = false;
    }
    application->view.daily_goal_reached =
        application->daily_goal_reached_for_completion;
    application->view.scheduled_mode =
        pomodoro_scheduled_next_mode(&application->model);
    application->view.auto_start_pending =
        application->model.timer.timer_state == POMODORO_TIMER_COMPLETION &&
        !application->model.timer.auto_start_cancelled &&
        application->model.timer.completion_transition_deadline_epoch_seconds != 0U;
    application->view.auto_start_seconds_remaining = 0U;
    if (application->view.auto_start_pending && time != NULL &&
        time->epoch_seconds <
            application->model.timer.completion_transition_deadline_epoch_seconds) {
        uint64_t seconds =
            application->model.timer.completion_transition_deadline_epoch_seconds -
            time->epoch_seconds;
        application->view.auto_start_seconds_remaining =
            seconds > UINT8_MAX ? UINT8_MAX : (uint8_t) seconds;
    }
}

static pomodoro_application_result_t fail_platform(
    pomodoro_application_t *application,
    pomodoro_application_error_t error)
{
    if (application->booted) {
        snapshot_view(application, NULL, POMODORO_EVENT_NONE);
    }
    application->view.phase = POMODORO_APPLICATION_PHASE_PLATFORM_ERROR;
    application->view.error = error;
    (void) application->platform.render(application->platform.context,
                                        &application->view);
    return POMODORO_APPLICATION_ERR_PLATFORM;
}

static pomodoro_application_result_t render(
    pomodoro_application_t *application,
    const pomodoro_application_time_t *time,
    pomodoro_event_t completion_event)
{
    snapshot_view(application, time, completion_event);
    if (!application->platform.render(application->platform.context,
                                      &application->view)) {
        return fail_platform(application, POMODORO_APPLICATION_ERROR_DISPLAY);
    }
    return POMODORO_APPLICATION_OK;
}

static pomodoro_application_result_t read_time(
    pomodoro_application_t *application,
    pomodoro_application_time_t *time)
{
    if (!application->platform.read_time(application->platform.context,
                                         time)) {
        return fail_platform(application, POMODORO_APPLICATION_ERROR_CLOCK);
    }
    return POMODORO_APPLICATION_OK;
}

static pomodoro_application_result_t save(
    pomodoro_application_t *application)
{
    pomodoro_persisted_state_t persisted;
    if (!pomodoro_persistence_pack(&persisted, &application->model,
                                   &application->statistics)) {
        return POMODORO_APPLICATION_ERR_INVALID_STATE;
    }
    if (!application->platform.save(application->platform.context,
                                    &persisted)) {
        return fail_platform(application,
                             POMODORO_APPLICATION_ERROR_STORAGE_SAVE);
    }
    return POMODORO_APPLICATION_OK;
}

static pomodoro_application_result_t publish_completion(
    pomodoro_application_t *application,
    const pomodoro_application_time_t *time,
    pomodoro_event_t event)
{
    application->view.confirmation = POMODORO_APPLICATION_CONFIRM_NONE;
    uint16_t completed_before = time->rtc_is_valid
        ? completed_pomodoros_on(&application->statistics, time->local_date)
        : 0U;
    bool reaches_daily_goal =
        event == POMODORO_EVENT_FOCUS_COMPLETE && time->rtc_is_valid &&
        completed_before < application->model.settings.daily_goal &&
        completed_before + 1U >= application->model.settings.daily_goal;
    if (!pomodoro_statistics_apply(
            &application->statistics,
            &application->model.timer.last_completion,
            time->rtc_is_valid, time->local_date)) {
        return POMODORO_APPLICATION_ERR_INVALID_STATE;
    }
    application->daily_goal_reached_for_completion = reaches_daily_goal;
    pomodoro_application_result_t result = save(application);
    if (result != POMODORO_APPLICATION_OK) {
        return result;
    }
    result = render(application, time, event);
    if (result != POMODORO_APPLICATION_OK) {
        return result;
    }
    bool audio_played = event == POMODORO_EVENT_FOCUS_COMPLETE
        ? application->platform.play_focus_complete(
              application->platform.context,
              application->model.settings.volume)
        : application->platform.play_break_complete(
              application->platform.context,
              application->model.settings.volume);
    return audio_played
        ? POMODORO_APPLICATION_OK
        : fail_platform(application, POMODORO_APPLICATION_ERROR_AUDIO);
}

static pomodoro_application_result_t save_and_render(
    pomodoro_application_t *application,
    const pomodoro_application_time_t *time)
{
    pomodoro_application_result_t result = save(application);
    return result == POMODORO_APPLICATION_OK
        ? render(application, time, POMODORO_EVENT_NONE)
        : result;
}

static pomodoro_application_result_t boot(
    pomodoro_application_t *application)
{
    pomodoro_application_time_t time;
    pomodoro_persisted_state_t persisted;
    pomodoro_application_result_t result = read_time(application, &time);
    if (result != POMODORO_APPLICATION_OK) {
        return result;
    }

    pomodoro_application_load_result_t loaded = application->platform.load(
        application->platform.context, &persisted);
    if (loaded == POMODORO_APPLICATION_LOAD_ERROR) {
        return fail_platform(application,
                             POMODORO_APPLICATION_ERROR_STORAGE_LOAD);
    }
    if (loaded == POMODORO_APPLICATION_LOAD_LOADED) {
        if (!pomodoro_persistence_unpack(&persisted, &application->model,
                                         &application->statistics)) {
            return fail_platform(application,
                                 POMODORO_APPLICATION_ERROR_STORAGE_LOAD);
        }
        if (application->model.timer.timer_state ==
            POMODORO_TIMER_COMPLETION) {
            pomodoro_event_t ignored_event = POMODORO_EVENT_NONE;
            pomodoro_snapshot_t snapshot = application->model.timer;
            if (pomodoro_restore(&application->model, &snapshot,
                                 time.epoch_seconds, &ignored_event) !=
                    POMODORO_OK || ignored_event != POMODORO_EVENT_NONE) {
                return POMODORO_APPLICATION_ERR_INVALID_STATE;
            }
            const pomodoro_daily_statistics_t *today = time.rtc_is_valid
                ? pomodoro_statistics_find(&application->statistics,
                                           time.local_date)
                : NULL;
            application->daily_goal_reached_for_completion =
                application->model.timer.last_completion.mode ==
                    POMODORO_MODE_FOCUS &&
                application->model.timer.last_completion.settlement_sequence ==
                    application->statistics.last_applied_settlement_sequence &&
                today != NULL &&
                today->completed_pomodoros ==
                    application->model.settings.daily_goal;
        }
    } else {
        pomodoro_settings_t settings = pomodoro_default_settings();
        if (pomodoro_init(&application->model, &settings) != POMODORO_OK) {
            return POMODORO_APPLICATION_ERR_INVALID_STATE;
        }
        pomodoro_statistics_init(&application->statistics);
    }
    application->view.phase = POMODORO_APPLICATION_PHASE_ACTIVE;
    application->view.error = POMODORO_APPLICATION_ERROR_NONE;
    application->booted = true;
    return render(application, &time, POMODORO_EVENT_NONE);
}

static pomodoro_application_result_t start(
    pomodoro_application_t *application)
{
    pomodoro_application_time_t time;
    pomodoro_application_result_t result = read_time(application, &time);
    if (result != POMODORO_APPLICATION_OK) {
        return result;
    }
    if (pomodoro_start(&application->model, time.epoch_seconds) != POMODORO_OK) {
        return POMODORO_APPLICATION_ERR_INVALID_STATE;
    }
    return save_and_render(application, &time);
}

static pomodoro_application_result_t pause_timer(
    pomodoro_application_t *application)
{
    pomodoro_application_time_t time;
    pomodoro_application_result_t result = read_time(application, &time);
    if (result != POMODORO_APPLICATION_OK) {
        return result;
    }
    pomodoro_event_t event = POMODORO_EVENT_NONE;
    if (pomodoro_pause(&application->model, time.epoch_seconds, &event) !=
        POMODORO_OK) {
        return POMODORO_APPLICATION_ERR_INVALID_STATE;
    }
    if (event != POMODORO_EVENT_NONE) {
        return publish_completion(application, &time, event);
    }
    return save_and_render(application, &time);
}

static pomodoro_application_result_t resume_timer(
    pomodoro_application_t *application)
{
    pomodoro_application_time_t time;
    pomodoro_application_result_t result = read_time(application, &time);
    if (result != POMODORO_APPLICATION_OK) {
        return result;
    }
    if (pomodoro_resume(&application->model, time.epoch_seconds) != POMODORO_OK) {
        return POMODORO_APPLICATION_ERR_INVALID_STATE;
    }
    return save_and_render(application, &time);
}

static pomodoro_application_result_t request_reset(
    pomodoro_application_t *application)
{
    if (application->model.timer.timer_state != POMODORO_TIMER_PAUSED) {
        return POMODORO_APPLICATION_ERR_INVALID_STATE;
    }
    application->view.confirmation = POMODORO_APPLICATION_CONFIRM_RESET;
    return render(application, NULL, POMODORO_EVENT_NONE);
}

static pomodoro_application_result_t cancel_confirmation(
    pomodoro_application_t *application)
{
    if (application->view.confirmation == POMODORO_APPLICATION_CONFIRM_NONE) {
        return POMODORO_APPLICATION_ERR_INVALID_STATE;
    }
    application->view.confirmation = POMODORO_APPLICATION_CONFIRM_NONE;
    return render(application, NULL, POMODORO_EVENT_NONE);
}

static pomodoro_application_result_t confirm_reset(
    pomodoro_application_t *application)
{
    if (application->view.confirmation != POMODORO_APPLICATION_CONFIRM_RESET ||
        pomodoro_reset(&application->model) != POMODORO_OK) {
        return POMODORO_APPLICATION_ERR_INVALID_STATE;
    }
    application->view.confirmation = POMODORO_APPLICATION_CONFIRM_NONE;
    return save_and_render(application, NULL);
}

static pomodoro_application_result_t request_mode(
    pomodoro_application_t *application, int direction)
{
    if (application->view.confirmation != POMODORO_APPLICATION_CONFIRM_NONE ||
        application->model.timer.timer_state == POMODORO_TIMER_COMPLETION) {
        return POMODORO_APPLICATION_ERR_INVALID_STATE;
    }
    int requested = (int) application->model.timer.mode + direction;
    if (requested < (int) POMODORO_MODE_FOCUS ||
        requested > (int) POMODORO_MODE_LONG_BREAK) {
        application->view.mode_boundary_feedback = true;
        return render(application, NULL, POMODORO_EVENT_NONE);
    }

    pomodoro_mode_t mode = (pomodoro_mode_t) requested;
    pomodoro_result_t core_result = pomodoro_request_mode(&application->model,
                                                          mode);
    if (core_result == POMODORO_ERR_CONFIRMATION_REQUIRED) {
        application->view.confirmation =
            POMODORO_APPLICATION_CONFIRM_MODE_SWITCH;
        application->view.pending_mode = mode;
        return render(application, NULL, POMODORO_EVENT_NONE);
    }
    if (core_result != POMODORO_OK) {
        return POMODORO_APPLICATION_ERR_INVALID_STATE;
    }
    return save_and_render(application, NULL);
}

static pomodoro_application_result_t confirm_mode_switch(
    pomodoro_application_t *application)
{
    if (application->view.confirmation !=
            POMODORO_APPLICATION_CONFIRM_MODE_SWITCH ||
        pomodoro_confirm_mode_switch(&application->model,
                                     application->view.pending_mode) !=
            POMODORO_OK) {
        return POMODORO_APPLICATION_ERR_INVALID_STATE;
    }
    application->view.confirmation = POMODORO_APPLICATION_CONFIRM_NONE;
    return save_and_render(application, NULL);
}

static pomodoro_application_result_t tick(
    pomodoro_application_t *application)
{
    pomodoro_application_time_t time;
    pomodoro_application_result_t result = read_time(application, &time);
    if (result != POMODORO_APPLICATION_OK) {
        return result;
    }

    if (pomodoro_auto_start_is_due(&application->model, time.epoch_seconds)) {
        if (pomodoro_start_next(&application->model, time.epoch_seconds) !=
            POMODORO_OK) {
            return POMODORO_APPLICATION_ERR_INVALID_STATE;
        }
        return save_and_render(application, &time);
    }

    pomodoro_event_t event = pomodoro_tick(&application->model,
                                           time.epoch_seconds);
    if (event != POMODORO_EVENT_NONE) {
        return publish_completion(application, &time, event);
    }
    return render(application, &time, POMODORO_EVENT_NONE);
}

static pomodoro_application_result_t start_next(
    pomodoro_application_t *application)
{
    pomodoro_application_time_t time;
    pomodoro_application_result_t result = read_time(application, &time);
    if (result != POMODORO_APPLICATION_OK) {
        return result;
    }
    if (pomodoro_start_next(&application->model, time.epoch_seconds) !=
        POMODORO_OK) {
        return POMODORO_APPLICATION_ERR_INVALID_STATE;
    }
    return save_and_render(application, &time);
}

static pomodoro_application_result_t cancel_auto_start(
    pomodoro_application_t *application)
{
    if (pomodoro_cancel_auto_start(&application->model) != POMODORO_OK) {
        return POMODORO_APPLICATION_ERR_INVALID_STATE;
    }
    if (pomodoro_finish_completion(&application->model) != POMODORO_OK) {
        return POMODORO_APPLICATION_ERR_INVALID_STATE;
    }
    return save_and_render(application, NULL);
}

static pomodoro_application_result_t not_now(
    pomodoro_application_t *application)
{
    if (pomodoro_finish_completion(&application->model) != POMODORO_OK) {
        return POMODORO_APPLICATION_ERR_INVALID_STATE;
    }
    return save_and_render(application, NULL);
}

pomodoro_application_result_t pomodoro_application_create(
    pomodoro_application_t **application,
    const pomodoro_application_platform_t *platform)
{
    if (application == NULL || !platform_is_valid(platform)) {
        return POMODORO_APPLICATION_ERR_INVALID_ARGUMENT;
    }
    *application = calloc(1, sizeof(**application));
    if (*application == NULL) {
        return POMODORO_APPLICATION_ERR_OUT_OF_MEMORY;
    }
    (*application)->platform = *platform;
    return POMODORO_APPLICATION_OK;
}

pomodoro_application_result_t pomodoro_application_dispatch(
    pomodoro_application_t *application,
    pomodoro_application_action_t action)
{
    if (application == NULL) {
        return POMODORO_APPLICATION_ERR_INVALID_ARGUMENT;
    }
    if (action == POMODORO_APPLICATION_ACTION_BOOT && !application->booted) {
        return boot(application);
    }
    if (!application->booted ||
        application->view.phase != POMODORO_APPLICATION_PHASE_ACTIVE) {
        return POMODORO_APPLICATION_ERR_INVALID_STATE;
    }
    application->view.mode_boundary_feedback = false;
    if (action == POMODORO_APPLICATION_ACTION_START) {
        return start(application);
    }
    if (action == POMODORO_APPLICATION_ACTION_TICK) {
        return tick(application);
    }
    if (action == POMODORO_APPLICATION_ACTION_PAUSE) {
        return pause_timer(application);
    }
    if (action == POMODORO_APPLICATION_ACTION_RESUME) {
        return resume_timer(application);
    }
    if (action == POMODORO_APPLICATION_ACTION_REQUEST_RESET) {
        return request_reset(application);
    }
    if (action == POMODORO_APPLICATION_ACTION_CONFIRM_RESET) {
        return confirm_reset(application);
    }
    if (action == POMODORO_APPLICATION_ACTION_CANCEL_CONFIRMATION) {
        return cancel_confirmation(application);
    }
    if (action == POMODORO_APPLICATION_ACTION_MODE_PREVIOUS) {
        return request_mode(application, -1);
    }
    if (action == POMODORO_APPLICATION_ACTION_MODE_NEXT) {
        return request_mode(application, 1);
    }
    if (action == POMODORO_APPLICATION_ACTION_CONFIRM_MODE_SWITCH) {
        return confirm_mode_switch(application);
    }
    if (action == POMODORO_APPLICATION_ACTION_START_NEXT) {
        return start_next(application);
    }
    if (action == POMODORO_APPLICATION_ACTION_CANCEL_AUTO_START) {
        return cancel_auto_start(application);
    }
    if (action == POMODORO_APPLICATION_ACTION_NOT_NOW) {
        return not_now(application);
    }
    return POMODORO_APPLICATION_ERR_INVALID_STATE;
}

void pomodoro_application_destroy(pomodoro_application_t *application)
{
    free(application);
}
