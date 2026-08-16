#include "pomodoro_application.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

struct pomodoro_application {
    pomodoro_application_platform_t platform;
    pomodoro_model_t model;
    pomodoro_statistics_store_t statistics;
    pomodoro_application_view_t view;
    bool booted;
};

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
    application->view.remaining_seconds = time != NULL
        ? pomodoro_remaining_seconds(&application->model, time->epoch_seconds)
        : application->model.timer.remaining_seconds;
    application->view.set_progress = application->model.timer.set_progress;
    application->view.settings = application->model.settings;
    application->view.completion_event = completion_event;
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
    result = save(application);
    if (result != POMODORO_APPLICATION_OK) {
        return result;
    }
    return render(application, &time, POMODORO_EVENT_NONE);
}

static pomodoro_application_result_t tick(
    pomodoro_application_t *application)
{
    pomodoro_application_time_t time;
    pomodoro_application_result_t result = read_time(application, &time);
    if (result != POMODORO_APPLICATION_OK) {
        return result;
    }

    pomodoro_event_t event = pomodoro_tick(&application->model,
                                           time.epoch_seconds);
    if (event != POMODORO_EVENT_NONE) {
        if (!pomodoro_statistics_apply(
                &application->statistics,
                &application->model.timer.last_completion,
                time.rtc_is_valid, time.local_date)) {
            return POMODORO_APPLICATION_ERR_INVALID_STATE;
        }
        result = save(application);
        if (result != POMODORO_APPLICATION_OK) {
            return result;
        }
        result = render(application, &time, event);
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
        if (!audio_played) {
            return fail_platform(application,
                                 POMODORO_APPLICATION_ERROR_AUDIO);
        }
        return POMODORO_APPLICATION_OK;
    }
    return render(application, &time, POMODORO_EVENT_NONE);
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
    if (action == POMODORO_APPLICATION_ACTION_START) {
        return start(application);
    }
    if (action == POMODORO_APPLICATION_ACTION_TICK) {
        return tick(application);
    }
    return POMODORO_APPLICATION_ERR_INVALID_STATE;
}

void pomodoro_application_destroy(pomodoro_application_t *application)
{
    free(application);
}
