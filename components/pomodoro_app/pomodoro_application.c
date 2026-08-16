#include "pomodoro_application.h"
#include "pomodoro_display_policy.h"
#include "pomodoro_settings_editor.h"
#include "pomodoro_statistics_projection.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

struct pomodoro_application {
    pomodoro_application_platform_t platform;
    pomodoro_model_t model;
    pomodoro_statistics_store_t statistics;
    pomodoro_persisted_state_t persistence_workspace;
    pomodoro_application_view_t view;
    bool daily_goal_reached_for_completion;
    bool booted;
    bool has_last_time;
    bool timer_restore_pending;
    pomodoro_application_time_t last_time;
    pomodoro_application_page_t page;
    pomodoro_application_page_t statistics_source_page;
    pomodoro_application_page_t settings_source_page;
    pomodoro_numeric_editor_t numeric_editor;
    uint8_t brightness_preview_percent;
    pomodoro_application_device_status_t device_status;
    bool low_battery_warning_shown;
    uint64_t low_battery_warning_deadline_epoch_seconds;
    uint64_t volume_feedback_deadline_epoch_seconds;
    uint64_t last_interaction_epoch_seconds;
    bool display_awake;
    bool display_manually_asleep;
    uint8_t effective_brightness_percent;
    pomodoro_application_manual_time_t manual_time_draft;
    bool time_setup_can_cancel;
    pomodoro_application_time_setup_stage_t time_setup_stage;
    pomodoro_application_time_sync_session_t time_sync_session;
    uint64_t time_sync_deadline_epoch_seconds;
    uint64_t time_sync_success_deadline_epoch_seconds;
    pomodoro_application_page_t time_sync_destination_page;
    bool time_sync_connected;
    bool time_sync_failed;
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
           platform->resolve_local_date != NULL &&
           platform->write_utc_time != NULL &&
           platform->set_display_brightness_percent != NULL &&
           platform->read_device_status != NULL &&
           platform->set_display_awake != NULL &&
           platform->power_off != NULL &&
           platform->start_time_sync != NULL &&
           platform->poll_time_sync != NULL &&
           platform->stop_time_sync != NULL &&
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
    application->view.remaining_seconds = time != NULL &&
            !application->timer_restore_pending
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
    const pomodoro_application_time_t *resolved_time = time != NULL
        ? time
        : (application->has_last_time ? &application->last_time : NULL);
    application->view.rtc_is_valid = time != NULL
        ? time->rtc_is_valid
        : application->has_last_time && application->last_time.rtc_is_valid;
    if (application->view.rtc_is_valid && resolved_time != NULL) {
        application->view.current_local_date = resolved_time->local_date;
        int16_t offset_minutes = pomodoro_time_zone_offset_minutes_at(
            &application->model.time_zone, resolved_time->epoch_seconds,
            application->model.settings.utc_offset_quarter_hours);
        int64_t local_epoch = (int64_t) resolved_time->epoch_seconds +
            (int64_t) offset_minutes * 60;
        int64_t seconds_of_day = local_epoch % 86400;
        if (seconds_of_day < 0) {
            seconds_of_day += 86400;
        }
        application->view.current_local_hour =
            (uint8_t) (seconds_of_day / 3600);
        application->view.current_local_minute =
            (uint8_t) ((seconds_of_day / 60) % 60);
    } else {
        memset(&application->view.current_local_date, 0,
               sizeof(application->view.current_local_date));
        application->view.current_local_hour = 0U;
        application->view.current_local_minute = 0U;
    }
    application->view.page = application->page;
    application->view.numeric_setting = application->numeric_editor.setting;
    application->view.numeric_draft_value = application->numeric_editor.value;
    application->view.numeric_minimum = application->numeric_editor.minimum;
    application->view.numeric_maximum = application->numeric_editor.maximum;
    application->view.brightness_preview_percent =
        application->brightness_preview_percent != 0U
        ? application->brightness_preview_percent
        : application->model.settings.brightness_percent;
    application->view.last_nonzero_volume =
        application->model.last_nonzero_volume;
    application->view.device_status = application->device_status;
    application->view.display_awake = application->display_awake;
    application->view.effective_brightness_percent =
        application->effective_brightness_percent;
    application->view.manual_time_draft = application->manual_time_draft;
    application->view.time_setup_stage = application->time_setup_stage;
    application->view.time_sync_session = application->time_sync_session;
    application->view.time_sync_connected = application->time_sync_connected;
    application->view.time_sync_failed = application->time_sync_failed;
    application->view.time_sync_seconds_remaining = 0U;
    if (resolved_time != NULL && application->time_sync_deadline_epoch_seconds >
                                     resolved_time->epoch_seconds) {
        uint64_t remaining = application->time_sync_deadline_epoch_seconds -
            resolved_time->epoch_seconds;
        application->view.time_sync_seconds_remaining = remaining > UINT16_MAX
            ? UINT16_MAX
            : (uint16_t) remaining;
    }
    application->view.time_zone_needs_sync = resolved_time != NULL &&
        pomodoro_time_zone_schedule_needs_sync(
            &application->model.time_zone, resolved_time->epoch_seconds);
    memcpy(application->view.time_zone_name,
           application->model.time_zone.iana_name,
           sizeof(application->view.time_zone_name));
    application->view.statistics_available = application->view.rtc_is_valid;
    pomodoro_statistics_project_view(
        &application->statistics,
        resolved_time != NULL ? resolved_time->local_date
                              : (pomodoro_date_t) {0},
        application->model.settings.daily_goal,
        &application->view);
    application->view.volume_feedback_visible = resolved_time != NULL &&
        resolved_time->epoch_seconds <
            application->volume_feedback_deadline_epoch_seconds;
    application->view.low_battery_warning_visible = resolved_time != NULL &&
        resolved_time->epoch_seconds <
            application->low_battery_warning_deadline_epoch_seconds;
    if (resolved_time != NULL) {
        pomodoro_display_policy_pixel_shift(
            resolved_time->epoch_seconds,
            &application->view.pixel_shift_x,
            &application->view.pixel_shift_y);
    }
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

static pomodoro_application_result_t save(
    pomodoro_application_t *application);

static pomodoro_application_result_t read_time(
    pomodoro_application_t *application,
    pomodoro_application_time_t *time)
{
    if (!application->platform.read_time(application->platform.context,
                                         time)) {
        return fail_platform(application, POMODORO_APPLICATION_ERROR_CLOCK);
    }
    if (time->rtc_is_valid &&
        !application->platform.resolve_local_date(
            application->platform.context, time->epoch_seconds,
            pomodoro_time_zone_offset_minutes_at(
                &application->model.time_zone, time->epoch_seconds,
                application->model.settings.utc_offset_quarter_hours),
            &time->local_date)) {
        return fail_platform(application, POMODORO_APPLICATION_ERROR_CLOCK);
    }
    pomodoro_application_device_status_t status;
    if (!application->platform.read_device_status(
            application->platform.context, &status) ||
        status.battery_percent > 100U) {
        return fail_platform(application, POMODORO_APPLICATION_ERROR_DISPLAY);
    }
    application->device_status = status;
    if (status.low_battery && !application->low_battery_warning_shown) {
        application->low_battery_warning_shown = true;
        application->low_battery_warning_deadline_epoch_seconds =
            time->epoch_seconds + 3U;
    }
    bool time_recovered = application->booted && application->has_last_time &&
        !application->last_time.rtc_is_valid && time->rtc_is_valid;
    application->last_time = *time;
    application->has_last_time = true;
    if (time_recovered) {
        if (!pomodoro_statistics_assign_pending(&application->statistics,
                                                time->local_date)) {
            return POMODORO_APPLICATION_ERR_INVALID_STATE;
        }
        pomodoro_application_result_t result = save(application);
        if (result != POMODORO_APPLICATION_OK) {
            return result;
        }
    }
    return POMODORO_APPLICATION_OK;
}

static pomodoro_application_result_t set_display_state(
    pomodoro_application_t *application, bool awake, uint8_t brightness)
{
    if (application->display_awake != awake &&
        !application->platform.set_display_awake(
            application->platform.context, awake)) {
        return fail_platform(application, POMODORO_APPLICATION_ERROR_DISPLAY);
    }
    application->display_awake = awake;
    if (awake && application->effective_brightness_percent != brightness &&
        !application->platform.set_display_brightness_percent(
            application->platform.context, brightness)) {
        return fail_platform(application, POMODORO_APPLICATION_ERROR_DISPLAY);
    }
    application->effective_brightness_percent = awake ? brightness : 0U;
    return POMODORO_APPLICATION_OK;
}

static pomodoro_application_result_t update_display_lifecycle(
    pomodoro_application_t *application,
    const pomodoro_application_time_t *time)
{
    if (application->display_manually_asleep) {
        return set_display_state(application, false, 0U);
    }
    uint64_t idle_seconds = time->epoch_seconds >=
            application->last_interaction_epoch_seconds
        ? time->epoch_seconds - application->last_interaction_epoch_seconds
        : 0U;
    pomodoro_display_policy_result_t policy = pomodoro_display_policy_resolve(
        application->model.timer.timer_state, idle_seconds,
        application->model.settings.brightness_percent);
    return set_display_state(application, policy.awake,
                             policy.brightness_percent);
}

static pomodoro_application_result_t record_interaction(
    pomodoro_application_t *application)
{
    if (!application->has_last_time) {
        return POMODORO_APPLICATION_OK;
    }
    pomodoro_application_result_t result = set_display_state(
        application, true, application->model.settings.brightness_percent);
    if (result != POMODORO_APPLICATION_OK) {
        return result;
    }
    application->display_manually_asleep = false;
    application->last_interaction_epoch_seconds =
        application->last_time.epoch_seconds;
    return POMODORO_APPLICATION_OK;
}

static pomodoro_application_result_t save(
    pomodoro_application_t *application)
{
    if (!pomodoro_persistence_pack(&application->persistence_workspace,
                                   &application->model,
                                   &application->statistics)) {
        return POMODORO_APPLICATION_ERR_INVALID_STATE;
    }
    if (!application->platform.save(application->platform.context,
                                    &application->persistence_workspace)) {
        return fail_platform(application,
                             POMODORO_APPLICATION_ERROR_STORAGE_SAVE);
    }
    return POMODORO_APPLICATION_OK;
}

static pomodoro_application_result_t publish_completion(
    pomodoro_application_t *application,
    const pomodoro_application_time_t *time,
    pomodoro_event_t event, bool completion_clock_was_valid)
{
    application->view.confirmation = POMODORO_APPLICATION_CONFIRM_NONE;
    application->page = POMODORO_APPLICATION_PAGE_MAIN;
    application->last_interaction_epoch_seconds = time->epoch_seconds;
    pomodoro_application_result_t wake_result = set_display_state(
        application, true, application->model.settings.brightness_percent);
    if (wake_result != POMODORO_APPLICATION_OK) {
        return wake_result;
    }
    application->display_manually_asleep = false;
    pomodoro_date_t completion_date = time->local_date;
    if (event == POMODORO_EVENT_FOCUS_COMPLETE &&
        completion_clock_was_valid &&
        !application->platform.resolve_local_date(
            application->platform.context,
            application->model.timer.deadline_epoch_seconds,
            pomodoro_time_zone_offset_minutes_at(
                &application->model.time_zone,
                application->model.timer.deadline_epoch_seconds,
                application->model.settings.utc_offset_quarter_hours),
            &completion_date)) {
        return fail_platform(application, POMODORO_APPLICATION_ERROR_CLOCK);
    }
    pomodoro_date_t statistics_date = completion_clock_was_valid
        ? completion_date
        : time->local_date;
    uint16_t completed_before = time->rtc_is_valid
        ? completed_pomodoros_on(&application->statistics, statistics_date)
        : 0U;
    bool reaches_daily_goal =
        event == POMODORO_EVENT_FOCUS_COMPLETE && time->rtc_is_valid &&
        completed_before < application->model.settings.daily_goal &&
        completed_before + 1U >= application->model.settings.daily_goal;
    if (!pomodoro_statistics_apply(
            &application->statistics,
            &application->model.timer.last_completion,
            completion_clock_was_valid, completion_date)) {
        return POMODORO_APPLICATION_ERR_INVALID_STATE;
    }
    if (!completion_clock_was_valid && time->rtc_is_valid &&
        !pomodoro_statistics_assign_pending(&application->statistics,
                                             statistics_date)) {
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
    pomodoro_event_t restored_event = POMODORO_EVENT_NONE;
    bool save_boot_state = false;
    pomodoro_application_load_result_t loaded = application->platform.load(
        application->platform.context, &application->persistence_workspace);
    if (loaded == POMODORO_APPLICATION_LOAD_ERROR) {
        return fail_platform(application,
                             POMODORO_APPLICATION_ERROR_STORAGE_LOAD);
    }
    if (loaded == POMODORO_APPLICATION_LOAD_LOADED) {
        bool was_migrated = false;
        if (!pomodoro_persistence_unpack(&application->persistence_workspace,
                                         &application->model,
                                         &application->statistics,
                                         &was_migrated)) {
            return fail_platform(application,
                                 POMODORO_APPLICATION_ERROR_STORAGE_LOAD);
        }
        save_boot_state = was_migrated;
    } else {
        pomodoro_settings_t settings = pomodoro_default_settings();
        if (pomodoro_init(&application->model, &settings) != POMODORO_OK) {
            return POMODORO_APPLICATION_ERR_INVALID_STATE;
        }
        pomodoro_statistics_init(&application->statistics);
        save_boot_state = true;
    }
    pomodoro_application_result_t result = read_time(application, &time);
    if (result != POMODORO_APPLICATION_OK) {
        return result;
    }
    application->display_awake = true;
    application->effective_brightness_percent =
        application->model.settings.brightness_percent;
    application->last_interaction_epoch_seconds = time.epoch_seconds;
    application->manual_time_draft = (pomodoro_application_manual_time_t) {
        .local_date = time.rtc_is_valid
            ? time.local_date
            : (pomodoro_date_t) {.year = 2026U, .month = 1U, .day = 1U},
        .hour = 12U,
        .minute = 0U,
        .utc_offset_quarter_hours =
            application->model.settings.utc_offset_quarter_hours,
    };
    if (!application->platform.set_display_awake(
            application->platform.context, true) ||
        !application->platform.set_display_brightness_percent(
            application->platform.context,
            application->model.settings.brightness_percent)) {
        return fail_platform(application, POMODORO_APPLICATION_ERROR_DISPLAY);
    }
    if (loaded == POMODORO_APPLICATION_LOAD_LOADED && time.rtc_is_valid) {
        pomodoro_snapshot_t snapshot = application->model.timer;
        if (pomodoro_restore(&application->model, &snapshot,
                             time.epoch_seconds, &restored_event) !=
            POMODORO_OK) {
            return fail_platform(application,
                                 POMODORO_APPLICATION_ERROR_STORAGE_LOAD);
        }
        if (application->model.timer.timer_state ==
            POMODORO_TIMER_COMPLETION) {
            const pomodoro_daily_statistics_t *today =
                pomodoro_statistics_find(&application->statistics,
                                         time.local_date);
            application->daily_goal_reached_for_completion =
                application->model.timer.last_completion.mode ==
                    POMODORO_MODE_FOCUS &&
                application->model.timer.last_completion.settlement_sequence ==
                    application->statistics.last_applied_settlement_sequence &&
                today != NULL &&
                today->completed_pomodoros ==
                    application->model.settings.daily_goal;
        }
    } else if (loaded == POMODORO_APPLICATION_LOAD_LOADED) {
        application->timer_restore_pending = true;
    }
    application->view.phase = time.rtc_is_valid
        ? POMODORO_APPLICATION_PHASE_ACTIVE
        : POMODORO_APPLICATION_PHASE_TIME_SETUP;
    application->time_setup_stage = time.rtc_is_valid
        ? POMODORO_APPLICATION_TIME_SETUP_MANUAL
        : POMODORO_APPLICATION_TIME_SETUP_LANDING;
    application->view.error = POMODORO_APPLICATION_ERROR_NONE;
    application->page = POMODORO_APPLICATION_PAGE_MAIN;
    application->booted = true;
    if (restored_event != POMODORO_EVENT_NONE) {
        return publish_completion(application, &time, restored_event, true);
    }
    if (save_boot_state) {
        result = save(application);
        if (result != POMODORO_APPLICATION_OK) {
            return result;
        }
    }
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
    bool clock_was_valid = application->has_last_time &&
        application->last_time.rtc_is_valid;
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
        return publish_completion(application, &time, event,
                                  clock_was_valid && time.rtc_is_valid);
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
    bool clock_was_valid = application->has_last_time &&
        application->last_time.rtc_is_valid;
    pomodoro_application_time_t time;
    pomodoro_application_result_t result = read_time(application, &time);
    if (result != POMODORO_APPLICATION_OK) {
        return result;
    }
    result = update_display_lifecycle(application, &time);
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
        return publish_completion(application, &time, event,
                                  clock_was_valid && time.rtc_is_valid);
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

static bool stop_time_sync_session(pomodoro_application_t *application)
{
    bool stopped = application->platform.stop_time_sync(
        application->platform.context);
    memset(&application->time_sync_session, 0,
           sizeof(application->time_sync_session));
    application->time_sync_deadline_epoch_seconds = 0U;
    application->time_sync_connected = false;
    return stopped;
}

static pomodoro_application_result_t start_time_sync(
    pomodoro_application_t *application)
{
    if (application->time_sync_deadline_epoch_seconds != 0U &&
        !stop_time_sync_session(application)) {
        return fail_platform(application, POMODORO_APPLICATION_ERROR_CLOCK);
    }
    memset(&application->time_sync_session, 0,
           sizeof(application->time_sync_session));
    if (!application->platform.start_time_sync(
            application->platform.context,
            &application->time_sync_session)) {
        (void) stop_time_sync_session(application);
        application->time_sync_failed = true;
        return render(application, NULL, POMODORO_EVENT_NONE);
    }
    application->time_setup_stage = POMODORO_APPLICATION_TIME_SETUP_SYNC;
    application->time_sync_deadline_epoch_seconds =
        application->last_time.epoch_seconds + 300U;
    application->time_sync_connected = false;
    application->time_sync_failed = false;
    return render(application, &application->last_time, POMODORO_EVENT_NONE);
}

static pomodoro_application_result_t fail_time_sync(
    pomodoro_application_t *application)
{
    if (!stop_time_sync_session(application)) {
        return fail_platform(application, POMODORO_APPLICATION_ERROR_CLOCK);
    }
    application->time_sync_failed = true;
    application->time_setup_stage = POMODORO_APPLICATION_TIME_SETUP_SYNC;
    return render(application, NULL, POMODORO_EVENT_NONE);
}

static pomodoro_application_result_t recover_timer_after_clock_set(
    pomodoro_application_t *application, uint64_t old_now_epoch_seconds,
    uint64_t new_now_epoch_seconds, pomodoro_event_t *restored_event)
{
    *restored_event = POMODORO_EVENT_NONE;
    if (application->model.timer.timer_state == POMODORO_TIMER_RUNNING &&
        !application->timer_restore_pending) {
        return pomodoro_rebase_running_timer(
                   &application->model, old_now_epoch_seconds,
                   new_now_epoch_seconds) == POMODORO_OK
            ? POMODORO_APPLICATION_OK
            : POMODORO_APPLICATION_ERR_INVALID_STATE;
    }
    if (!application->timer_restore_pending) {
        return POMODORO_APPLICATION_OK;
    }
    pomodoro_result_t result;
    if (application->model.timer.timer_state == POMODORO_TIMER_RUNNING) {
        result = pomodoro_rebase_running_timer_from_snapshot(
            &application->model, new_now_epoch_seconds);
    } else {
        pomodoro_snapshot_t snapshot = application->model.timer;
        result = pomodoro_restore(&application->model, &snapshot,
                                  new_now_epoch_seconds, restored_event);
    }
    if (result != POMODORO_OK) {
        return POMODORO_APPLICATION_ERR_INVALID_STATE;
    }
    application->timer_restore_pending = false;
    return POMODORO_APPLICATION_OK;
}

static pomodoro_application_result_t complete_time_sync(
    pomodoro_application_t *application,
    const pomodoro_application_time_sync_submission_t *submission)
{
    if (submission == NULL || submission->utc_epoch_seconds == 0U ||
        !pomodoro_time_zone_schedule_is_valid(&submission->time_zone) ||
        submission->utc_epoch_seconds >
            UINT64_MAX - POMODORO_TIME_ZONE_MINIMUM_FUTURE_SECONDS ||
        submission->time_zone.expires_utc_epoch_seconds <
            submission->utc_epoch_seconds +
                POMODORO_TIME_ZONE_MINIMUM_FUTURE_SECONDS) {
        return fail_time_sync(application);
    }
    uint64_t old_now_epoch_seconds = application->last_time.epoch_seconds;
    if (!application->platform.write_utc_time(
            application->platform.context, submission->utc_epoch_seconds)) {
        (void) stop_time_sync_session(application);
        return fail_platform(application, POMODORO_APPLICATION_ERROR_CLOCK);
    }
    pomodoro_application_time_t time;
    if (!application->platform.read_time(application->platform.context,
                                         &time)) {
        (void) stop_time_sync_session(application);
        return fail_platform(application, POMODORO_APPLICATION_ERROR_CLOCK);
    }
    time.rtc_is_valid = true;
    application->model.time_zone = submission->time_zone;
    int16_t offset_minutes = pomodoro_time_zone_offset_minutes_at(
        &application->model.time_zone, time.epoch_seconds,
        application->model.settings.utc_offset_quarter_hours);
    if (offset_minutes % 15 != 0 ||
        !application->platform.resolve_local_date(
            application->platform.context, time.epoch_seconds,
            offset_minutes, &time.local_date)) {
        (void) stop_time_sync_session(application);
        return fail_platform(application, POMODORO_APPLICATION_ERROR_CLOCK);
    }
    application->model.settings.utc_offset_quarter_hours =
        (int8_t) (offset_minutes / 15);
    application->last_time = time;
    application->has_last_time = true;
    pomodoro_event_t restored_event;
    pomodoro_application_result_t recovery_result =
        recover_timer_after_clock_set(
            application, old_now_epoch_seconds, time.epoch_seconds,
            &restored_event);
    if (recovery_result != POMODORO_APPLICATION_OK) {
        (void) stop_time_sync_session(application);
        return recovery_result;
    }
    if (!pomodoro_statistics_assign_pending(&application->statistics,
                                            time.local_date) ||
        !stop_time_sync_session(application)) {
        return fail_platform(application, POMODORO_APPLICATION_ERROR_CLOCK);
    }
    application->view.error = POMODORO_APPLICATION_ERROR_NONE;
    application->time_sync_destination_page = application->time_setup_can_cancel
        ? POMODORO_APPLICATION_PAGE_SETTINGS
        : POMODORO_APPLICATION_PAGE_MAIN;
    application->time_setup_can_cancel = false;
    application->time_sync_failed = false;
    if (restored_event != POMODORO_EVENT_NONE) {
        application->view.phase = POMODORO_APPLICATION_PHASE_ACTIVE;
        application->page = application->time_sync_destination_page;
        return publish_completion(application, &time, restored_event,
                                  time.rtc_is_valid);
    }
    application->view.phase = POMODORO_APPLICATION_PHASE_TIME_SETUP;
    application->time_setup_stage =
        POMODORO_APPLICATION_TIME_SETUP_SYNC_SUCCESS;
    application->time_sync_success_deadline_epoch_seconds =
        time.epoch_seconds + 1U;
    return save_and_render(application, &time);
}

static pomodoro_application_result_t finish_time_sync_success(
    pomodoro_application_t *application)
{
    pomodoro_application_time_t time;
    pomodoro_application_result_t result = read_time(application, &time);
    if (result != POMODORO_APPLICATION_OK) {
        return result;
    }
    if (time.epoch_seconds <
        application->time_sync_success_deadline_epoch_seconds) {
        return render(application, &time, POMODORO_EVENT_NONE);
    }
    application->time_sync_success_deadline_epoch_seconds = 0U;
    application->view.phase = POMODORO_APPLICATION_PHASE_ACTIVE;
    application->page = application->time_sync_destination_page;
    return render(application, &time, POMODORO_EVENT_NONE);
}

static pomodoro_application_result_t poll_time_sync(
    pomodoro_application_t *application)
{
    pomodoro_application_time_t time;
    pomodoro_application_result_t result = read_time(application, &time);
    if (result != POMODORO_APPLICATION_OK) {
        return result;
    }
    if (application->time_sync_deadline_epoch_seconds == 0U ||
        time.epoch_seconds >= application->time_sync_deadline_epoch_seconds) {
        return fail_time_sync(application);
    }
    pomodoro_application_time_sync_submission_t submission;
    memset(&submission, 0, sizeof(submission));
    pomodoro_application_time_sync_poll_result_t poll =
        application->platform.poll_time_sync(
            application->platform.context, &submission);
    if (poll == POMODORO_APPLICATION_TIME_SYNC_SUBMITTED) {
        return complete_time_sync(application, &submission);
    }
    if (poll == POMODORO_APPLICATION_TIME_SYNC_ERROR) {
        return fail_time_sync(application);
    }
    application->time_sync_connected =
        poll == POMODORO_APPLICATION_TIME_SYNC_CONNECTED;
    return render(application, &time, POMODORO_EVENT_NONE);
}

static pomodoro_application_result_t adjust_volume(
    pomodoro_application_t *application,
    pomodoro_application_action_t action)
{
    int delta = action == POMODORO_APPLICATION_ACTION_VOLUME_UP ? 1 : -1;
    int volume = (int) application->model.settings.volume + delta;
    if (volume < 0) {
        volume = 0;
    } else if (volume > 5) {
        volume = 5;
    }
    application->model.settings.volume = (uint8_t) volume;
    if (volume > 0) {
        application->model.last_nonzero_volume = (uint8_t) volume;
    }
    application->volume_feedback_deadline_epoch_seconds =
        application->last_time.epoch_seconds + 2U;
    return save_and_render(application, &application->last_time);
}

static pomodoro_application_result_t toggle_display(
    pomodoro_application_t *application)
{
    bool awake = !application->display_awake;
    pomodoro_application_result_t result = set_display_state(
        application, awake, application->model.settings.brightness_percent);
    if (result != POMODORO_APPLICATION_OK) {
        return result;
    }
    application->display_manually_asleep = !awake;
    if (awake) {
        application->last_interaction_epoch_seconds =
            application->last_time.epoch_seconds;
    }
    return render(application, NULL, POMODORO_EVENT_NONE);
}

static pomodoro_application_result_t power_off(
    pomodoro_application_t *application)
{
    return application->platform.power_off(application->platform.context)
        ? POMODORO_APPLICATION_OK
        : fail_platform(application, POMODORO_APPLICATION_ERROR_DISPLAY);
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
    if (application->booted &&
        application->view.phase == POMODORO_APPLICATION_PHASE_TIME_SETUP) {
        if (action == POMODORO_APPLICATION_ACTION_OPEN_TIME_SYNC) {
            return start_time_sync(application);
        }
        if (action == POMODORO_APPLICATION_ACTION_OPEN_DATE_TIME) {
            if (application->time_setup_stage ==
                    POMODORO_APPLICATION_TIME_SETUP_SYNC &&
                application->time_sync_deadline_epoch_seconds != 0U &&
                !stop_time_sync_session(application)) {
                return fail_platform(
                    application, POMODORO_APPLICATION_ERROR_CLOCK);
            }
            application->time_setup_stage =
                POMODORO_APPLICATION_TIME_SETUP_MANUAL;
            return render(application, NULL, POMODORO_EVENT_NONE);
        }
        if (action == POMODORO_APPLICATION_ACTION_TICK &&
            application->time_setup_stage ==
                POMODORO_APPLICATION_TIME_SETUP_SYNC) {
            return poll_time_sync(application);
        }
        if (action == POMODORO_APPLICATION_ACTION_TICK &&
            application->time_setup_stage ==
                POMODORO_APPLICATION_TIME_SETUP_SYNC_SUCCESS) {
            return finish_time_sync_success(application);
        }
        if (application->time_setup_stage ==
                POMODORO_APPLICATION_TIME_SETUP_MANUAL &&
            action >= POMODORO_APPLICATION_ACTION_MANUAL_YEAR_DOWN &&
            action <= POMODORO_APPLICATION_ACTION_MANUAL_OFFSET_UP) {
            if (!pomodoro_manual_time_adjust(
                    &application->manual_time_draft, action)) {
                return POMODORO_APPLICATION_ERR_INVALID_STATE;
            }
            return render(application, NULL, POMODORO_EVENT_NONE);
        }
        if (application->time_setup_stage ==
                POMODORO_APPLICATION_TIME_SETUP_MANUAL &&
            action == POMODORO_APPLICATION_ACTION_MANUAL_TIME_CONFIRM) {
            return pomodoro_application_confirm_manual_time(
                application, &application->manual_time_draft);
        }
        if (action == POMODORO_APPLICATION_ACTION_VOLUME_UP ||
            action == POMODORO_APPLICATION_ACTION_VOLUME_DOWN) {
            return adjust_volume(application, action);
        }
        if (action == POMODORO_APPLICATION_ACTION_TOGGLE_DISPLAY) {
            return toggle_display(application);
        }
        if (action == POMODORO_APPLICATION_ACTION_POWER_OFF) {
            return power_off(application);
        }
        if (action == POMODORO_APPLICATION_ACTION_BACK) {
            if (application->time_setup_stage ==
                    POMODORO_APPLICATION_TIME_SETUP_SYNC &&
                application->time_sync_deadline_epoch_seconds != 0U &&
                !stop_time_sync_session(application)) {
                return fail_platform(
                    application, POMODORO_APPLICATION_ERROR_CLOCK);
            }
            if (application->time_setup_can_cancel) {
                application->view.phase = POMODORO_APPLICATION_PHASE_ACTIVE;
                application->page = POMODORO_APPLICATION_PAGE_SETTINGS;
                application->time_setup_can_cancel = false;
            } else {
                application->time_setup_stage =
                    POMODORO_APPLICATION_TIME_SETUP_LANDING;
            }
            application->time_sync_failed = false;
            return render(application, NULL, POMODORO_EVENT_NONE);
        }
        return POMODORO_APPLICATION_ERR_INVALID_STATE;
    }
    if (!application->booted ||
        application->view.phase != POMODORO_APPLICATION_PHASE_ACTIVE) {
        return POMODORO_APPLICATION_ERR_INVALID_STATE;
    }
    application->view.mode_boundary_feedback = false;
    if (action != POMODORO_APPLICATION_ACTION_TICK &&
        action != POMODORO_APPLICATION_ACTION_TOGGLE_DISPLAY &&
        action != POMODORO_APPLICATION_ACTION_POWER_OFF) {
        pomodoro_application_result_t interaction =
            record_interaction(application);
        if (interaction != POMODORO_APPLICATION_OK) {
            return interaction;
        }
    }
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
    if (action == POMODORO_APPLICATION_ACTION_OPEN_MENU &&
        application->page == POMODORO_APPLICATION_PAGE_MAIN) {
        application->page = POMODORO_APPLICATION_PAGE_MENU;
        return render(application, NULL, POMODORO_EVENT_NONE);
    }
    if (action == POMODORO_APPLICATION_ACTION_OPEN_STATISTICS &&
        (application->page == POMODORO_APPLICATION_PAGE_MAIN ||
         application->page == POMODORO_APPLICATION_PAGE_MENU ||
         application->page == POMODORO_APPLICATION_PAGE_SETTINGS)) {
        application->statistics_source_page = application->page;
        application->page = POMODORO_APPLICATION_PAGE_STATISTICS;
        return render(application, NULL, POMODORO_EVENT_NONE);
    }
    if (action == POMODORO_APPLICATION_ACTION_OPEN_SETTINGS &&
        (application->page == POMODORO_APPLICATION_PAGE_MAIN ||
         application->page == POMODORO_APPLICATION_PAGE_MENU)) {
        application->settings_source_page = application->page;
        application->page = POMODORO_APPLICATION_PAGE_SETTINGS;
        return render(application, NULL, POMODORO_EVENT_NONE);
    }
    if (action == POMODORO_APPLICATION_ACTION_BACK) {
        if (application->page == POMODORO_APPLICATION_PAGE_STATISTICS) {
            application->page = application->statistics_source_page;
            return render(application, NULL, POMODORO_EVENT_NONE);
        }
        if (application->page == POMODORO_APPLICATION_PAGE_SETTINGS) {
            application->page = application->settings_source_page;
            return render(application, NULL, POMODORO_EVENT_NONE);
        }
        if (application->page == POMODORO_APPLICATION_PAGE_MENU) {
            application->page = POMODORO_APPLICATION_PAGE_MAIN;
            return render(application, NULL, POMODORO_EVENT_NONE);
        }
        if (application->page ==
            POMODORO_APPLICATION_PAGE_NUMERIC_STEPPER) {
            application->page = POMODORO_APPLICATION_PAGE_SETTINGS;
            return render(application, NULL, POMODORO_EVENT_NONE);
        }
    }
    if (action == POMODORO_APPLICATION_ACTION_VOLUME_UP ||
        action == POMODORO_APPLICATION_ACTION_VOLUME_DOWN) {
        return adjust_volume(application, action);
    }
    if (action == POMODORO_APPLICATION_ACTION_TOGGLE_SOUND) {
        if (application->model.settings.volume == 0U) {
            application->model.settings.volume =
                application->model.last_nonzero_volume;
        } else {
            application->model.last_nonzero_volume =
                application->model.settings.volume;
            application->model.settings.volume = 0U;
        }
        return save_and_render(application, &application->last_time);
    }
    if (action == POMODORO_APPLICATION_ACTION_TOGGLE_DISPLAY) {
        return toggle_display(application);
    }
    if (action == POMODORO_APPLICATION_ACTION_POWER_OFF) {
        return power_off(application);
    }
    if (action == POMODORO_APPLICATION_ACTION_OPEN_DATE_TIME &&
        (application->page == POMODORO_APPLICATION_PAGE_SETTINGS ||
         (application->page == POMODORO_APPLICATION_PAGE_STATISTICS &&
          !application->view.rtc_is_valid))) {
        application->manual_time_draft =
            (pomodoro_application_manual_time_t) {
                .local_date = application->view.current_local_date,
                .hour = application->view.current_local_hour,
                .minute = application->view.current_local_minute,
                .utc_offset_quarter_hours =
                    application->model.settings.utc_offset_quarter_hours,
            };
        application->time_setup_can_cancel = true;
        application->time_setup_stage =
            POMODORO_APPLICATION_TIME_SETUP_MANUAL;
        application->view.phase = POMODORO_APPLICATION_PHASE_TIME_SETUP;
        return render(application, NULL, POMODORO_EVENT_NONE);
    }
    if (action == POMODORO_APPLICATION_ACTION_OPEN_TIME_SYNC &&
        application->page == POMODORO_APPLICATION_PAGE_SETTINGS) {
        application->time_setup_can_cancel = true;
        application->view.phase = POMODORO_APPLICATION_PHASE_TIME_SETUP;
        return start_time_sync(application);
    }
    if (action >= POMODORO_APPLICATION_ACTION_OPEN_FOCUS_MINUTES &&
        action <= POMODORO_APPLICATION_ACTION_OPEN_DAILY_GOAL) {
        return pomodoro_application_open_numeric_setting(
            application,
            (pomodoro_application_numeric_setting_t) (
                action - POMODORO_APPLICATION_ACTION_OPEN_FOCUS_MINUTES));
    }
    if (action == POMODORO_APPLICATION_ACTION_NUMERIC_DECREMENT ||
        action == POMODORO_APPLICATION_ACTION_NUMERIC_INCREMENT) {
        return pomodoro_application_adjust_numeric_setting(
            application,
            action == POMODORO_APPLICATION_ACTION_NUMERIC_INCREMENT ? 1 : -1);
    }
    if (action == POMODORO_APPLICATION_ACTION_NUMERIC_CONFIRM) {
        return pomodoro_application_confirm_numeric_setting(application);
    }
    if (action == POMODORO_APPLICATION_ACTION_TOGGLE_AUTO_FOCUS ||
        action == POMODORO_APPLICATION_ACTION_TOGGLE_AUTO_BREAK) {
        pomodoro_settings_t settings = application->model.settings;
        if (action == POMODORO_APPLICATION_ACTION_TOGGLE_AUTO_FOCUS) {
            settings.auto_start_focus = !settings.auto_start_focus;
        } else {
            settings.auto_start_breaks = !settings.auto_start_breaks;
        }
        return pomodoro_application_confirm_settings(application, &settings);
    }
    return POMODORO_APPLICATION_ERR_INVALID_STATE;
}

pomodoro_application_result_t pomodoro_application_open_numeric_setting(
    pomodoro_application_t *application,
    pomodoro_application_numeric_setting_t setting)
{
    if (application == NULL) {
        return POMODORO_APPLICATION_ERR_INVALID_ARGUMENT;
    }
    if (!application->booted ||
        application->view.phase != POMODORO_APPLICATION_PHASE_ACTIVE ||
        application->page != POMODORO_APPLICATION_PAGE_SETTINGS ||
        !pomodoro_numeric_editor_open(&application->numeric_editor,
                                      &application->model.settings,
                                      setting)) {
        return POMODORO_APPLICATION_ERR_INVALID_STATE;
    }
    application->page = POMODORO_APPLICATION_PAGE_NUMERIC_STEPPER;
    return render(application, NULL, POMODORO_EVENT_NONE);
}

pomodoro_application_result_t pomodoro_application_adjust_numeric_setting(
    pomodoro_application_t *application, int delta)
{
    if (application == NULL) {
        return POMODORO_APPLICATION_ERR_INVALID_ARGUMENT;
    }
    if (application->page != POMODORO_APPLICATION_PAGE_NUMERIC_STEPPER ||
        !pomodoro_numeric_editor_adjust(&application->numeric_editor, delta)) {
        return POMODORO_APPLICATION_ERR_INVALID_STATE;
    }
    return render(application, NULL, POMODORO_EVENT_NONE);
}

pomodoro_application_result_t pomodoro_application_confirm_numeric_setting(
    pomodoro_application_t *application)
{
    if (application == NULL) {
        return POMODORO_APPLICATION_ERR_INVALID_ARGUMENT;
    }
    if (application->page != POMODORO_APPLICATION_PAGE_NUMERIC_STEPPER) {
        return POMODORO_APPLICATION_ERR_INVALID_STATE;
    }
    pomodoro_settings_t settings = application->model.settings;
    pomodoro_numeric_editor_apply(&application->numeric_editor, &settings);
    application->page = POMODORO_APPLICATION_PAGE_SETTINGS;
    return pomodoro_application_confirm_settings(application, &settings);
}

pomodoro_application_result_t pomodoro_application_preview_brightness(
    pomodoro_application_t *application, uint8_t percent)
{
    if (application == NULL || percent < 10U || percent > 100U ||
        percent % 5U != 0U) {
        return POMODORO_APPLICATION_ERR_INVALID_ARGUMENT;
    }
    if (application->page != POMODORO_APPLICATION_PAGE_SETTINGS) {
        return POMODORO_APPLICATION_ERR_INVALID_STATE;
    }
    if (!application->platform.set_display_brightness_percent(
            application->platform.context, percent)) {
        return fail_platform(application, POMODORO_APPLICATION_ERROR_DISPLAY);
    }
    application->brightness_preview_percent = percent;
    application->effective_brightness_percent = percent;
    return render(application, NULL, POMODORO_EVENT_NONE);
}

pomodoro_application_result_t pomodoro_application_confirm_brightness(
    pomodoro_application_t *application, uint8_t percent)
{
    pomodoro_application_result_t result =
        pomodoro_application_preview_brightness(application, percent);
    if (result != POMODORO_APPLICATION_OK) {
        return result;
    }
    pomodoro_settings_t settings = application->model.settings;
    settings.brightness_percent = percent;
    application->brightness_preview_percent = 0U;
    return pomodoro_application_confirm_settings(application, &settings);
}

pomodoro_application_result_t pomodoro_application_confirm_settings(
    pomodoro_application_t *application,
    const pomodoro_settings_t *settings)
{
    if (application == NULL || settings == NULL) {
        return POMODORO_APPLICATION_ERR_INVALID_ARGUMENT;
    }
    if (!application->booted ||
        application->view.phase != POMODORO_APPLICATION_PHASE_ACTIVE) {
        return POMODORO_APPLICATION_ERR_INVALID_STATE;
    }
    if (!pomodoro_settings_are_valid(settings)) {
        return POMODORO_APPLICATION_ERR_INVALID_ARGUMENT;
    }

    if (settings->volume == 0U && application->model.settings.volume > 0U) {
        application->model.last_nonzero_volume =
            application->model.settings.volume;
    } else if (settings->volume > 0U) {
        application->model.last_nonzero_volume = settings->volume;
    }
    application->model.settings = *settings;
    if (application->model.timer.timer_state == POMODORO_TIMER_READY) {
        application->model.timer.configured_duration_seconds =
            pomodoro_duration_seconds(&application->model,
                                      application->model.timer.mode);
        application->model.timer.remaining_seconds =
            application->model.timer.configured_duration_seconds;
    }
    return save_and_render(application, NULL);
}

pomodoro_application_result_t pomodoro_application_confirm_manual_time(
    pomodoro_application_t *application,
    const pomodoro_application_manual_time_t *manual_time)
{
    if (application == NULL || manual_time == NULL) {
        return POMODORO_APPLICATION_ERR_INVALID_ARGUMENT;
    }
    if (!application->booted ||
        (application->view.phase != POMODORO_APPLICATION_PHASE_ACTIVE &&
         application->view.phase != POMODORO_APPLICATION_PHASE_TIME_SETUP)) {
        return POMODORO_APPLICATION_ERR_INVALID_STATE;
    }
    uint64_t utc_epoch_seconds = 0U;
    if (!pomodoro_local_datetime_to_utc(
            manual_time->local_date, manual_time->hour, manual_time->minute,
            manual_time->utc_offset_quarter_hours, &utc_epoch_seconds)) {
        return POMODORO_APPLICATION_ERR_INVALID_ARGUMENT;
    }

    uint64_t old_now_epoch_seconds = application->last_time.epoch_seconds;
    if (!application->platform.write_utc_time(
            application->platform.context, utc_epoch_seconds)) {
        return fail_platform(application, POMODORO_APPLICATION_ERROR_CLOCK);
    }

    pomodoro_application_time_t time;
    if (!application->platform.read_time(application->platform.context, &time)) {
        return fail_platform(application, POMODORO_APPLICATION_ERROR_CLOCK);
    }
    time.rtc_is_valid = true;
    time.local_date = manual_time->local_date;
    application->last_time = time;
    application->has_last_time = true;
    application->model.settings.utc_offset_quarter_hours =
        manual_time->utc_offset_quarter_hours;
    memset(&application->model.time_zone, 0,
           sizeof(application->model.time_zone));
    pomodoro_event_t restored_event;
    pomodoro_application_result_t recovery_result =
        recover_timer_after_clock_set(
            application, old_now_epoch_seconds, time.epoch_seconds,
            &restored_event);
    if (recovery_result != POMODORO_APPLICATION_OK) {
        return recovery_result;
    }
    if (!pomodoro_statistics_assign_pending(&application->statistics,
                                            manual_time->local_date)) {
        return POMODORO_APPLICATION_ERR_INVALID_STATE;
    }
    application->view.phase = POMODORO_APPLICATION_PHASE_ACTIVE;
    application->view.error = POMODORO_APPLICATION_ERROR_NONE;
    application->time_setup_can_cancel = false;
    application->time_setup_stage = POMODORO_APPLICATION_TIME_SETUP_MANUAL;
    if (restored_event != POMODORO_EVENT_NONE) {
        return publish_completion(application, &time, restored_event,
                                  time.rtc_is_valid);
    }
    return save_and_render(application, &time);
}

pomodoro_application_result_t pomodoro_application_handle_touch(
    pomodoro_application_t *application, bool *consumed)
{
    if (application == NULL || consumed == NULL) {
        return POMODORO_APPLICATION_ERR_INVALID_ARGUMENT;
    }
    if (!application->booted ||
        application->view.phase == POMODORO_APPLICATION_PHASE_PLATFORM_ERROR) {
        return POMODORO_APPLICATION_ERR_INVALID_STATE;
    }
    *consumed = !application->display_awake;
    pomodoro_application_result_t result = record_interaction(application);
    if (result != POMODORO_APPLICATION_OK) {
        return result;
    }
    return render(application, NULL, POMODORO_EVENT_NONE);
}

void pomodoro_application_destroy(pomodoro_application_t *application)
{
    if (application != NULL &&
        application->time_sync_deadline_epoch_seconds != 0U) {
        (void) stop_time_sync_session(application);
    }
    free(application);
}
