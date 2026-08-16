#include "pomodoro_application.h"
#include "pomodoro_test_support.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    pomodoro_test_memory_journal_t memory_journal;
    pomodoro_journal_t journal;
    bool use_journal;
    uint64_t now_epoch_seconds;
    pomodoro_date_t local_date;
    pomodoro_date_t resolved_date;
    bool use_resolved_date;
    pomodoro_application_load_result_t load_result;
    pomodoro_persisted_state_t persisted;
    pomodoro_application_view_t rendered_view;
    unsigned render_count;
    unsigned save_count;
    unsigned focus_audio_count;
    unsigned break_audio_count;
    unsigned focus_completion_event_count;
    unsigned break_completion_event_count;
    bool fail_read_time;
    bool fail_save;
    bool fail_render;
    bool fail_focus_audio;
} fake_platform_t;

static void fake_enable_journal(fake_platform_t *fake)
{
    fake->use_journal = true;
    pomodoro_test_memory_journal_init(&fake->memory_journal, &fake->journal);
}

static bool fake_read_time(void *context, pomodoro_application_time_t *time)
{
    fake_platform_t *fake = context;
    if (fake->fail_read_time) {
        return false;
    }
    *time = (pomodoro_application_time_t) {
        .epoch_seconds = fake->now_epoch_seconds,
        .rtc_is_valid = true,
        .local_date = pomodoro_date_is_valid(fake->local_date)
            ? fake->local_date
            : (pomodoro_date_t) {.year = 2026, .month = 8, .day = 15},
    };
    return true;
}

static bool fake_resolve_local_date(void *context, uint64_t epoch_seconds,
                                    pomodoro_date_t *date)
{
    fake_platform_t *fake = context;
    (void) epoch_seconds;
    if (date == NULL) {
        return false;
    }
    *date = fake->use_resolved_date
        ? fake->resolved_date
        : (pomodoro_date_is_valid(fake->local_date)
               ? fake->local_date
               : (pomodoro_date_t) {.year = 2026, .month = 8, .day = 15});
    return pomodoro_date_is_valid(*date);
}

static pomodoro_application_load_result_t fake_load(
    void *context, pomodoro_persisted_state_t *state)
{
    fake_platform_t *fake = context;
    if (fake->use_journal) {
        pomodoro_journal_read_result_t result =
            pomodoro_journal_load(&fake->journal, state);
        if (result == POMODORO_JOURNAL_READ_LOADED) {
            return POMODORO_APPLICATION_LOAD_LOADED;
        }
        return result == POMODORO_JOURNAL_READ_EMPTY
            ? POMODORO_APPLICATION_LOAD_EMPTY
            : POMODORO_APPLICATION_LOAD_ERROR;
    }
    if (fake->load_result == POMODORO_APPLICATION_LOAD_LOADED) {
        *state = fake->persisted;
    }
    return fake->load_result;
}

static bool fake_save(void *context,
                      const pomodoro_persisted_state_t *state)
{
    fake_platform_t *fake = context;
    fake->save_count++;
    if (fake->fail_save) {
        return false;
    }
    if (fake->use_journal) {
        return pomodoro_journal_save(&fake->journal, state);
    }
    fake->persisted = *state;
    fake->load_result = POMODORO_APPLICATION_LOAD_LOADED;
    return true;
}

static bool fake_render(void *context,
                        const pomodoro_application_view_t *view)
{
    fake_platform_t *fake = context;
    fake->rendered_view = *view;
    fake->render_count++;
    if (view->completion_event == POMODORO_EVENT_FOCUS_COMPLETE) {
        fake->focus_completion_event_count++;
    } else if (view->completion_event == POMODORO_EVENT_BREAK_COMPLETE) {
        fake->break_completion_event_count++;
    }
    return !fake->fail_render;
}

static bool fake_play_focus_complete(void *context, uint8_t volume)
{
    fake_platform_t *fake = context;
    (void) volume;
    fake->focus_audio_count++;
    return !fake->fail_focus_audio;
}

static bool fake_play_break_complete(void *context, uint8_t volume)
{
    fake_platform_t *fake = context;
    (void) volume;
    fake->break_audio_count++;
    return true;
}

static pomodoro_application_platform_t fake_platform_services(
    fake_platform_t *fake)
{
    return (pomodoro_application_platform_t) {
        .context = fake,
        .read_time = fake_read_time,
        .resolve_local_date = fake_resolve_local_date,
        .load = fake_load,
        .save = fake_save,
        .render = fake_render,
        .play_focus_complete = fake_play_focus_complete,
        .play_break_complete = fake_play_break_complete,
    };
}

static pomodoro_application_t *create_application(fake_platform_t *fake)
{
    pomodoro_application_t *application = NULL;
    pomodoro_application_platform_t platform = fake_platform_services(fake);
    assert(pomodoro_application_create(&application, &platform) ==
           POMODORO_APPLICATION_OK);
    assert(application != NULL);
    return application;
}

static pomodoro_application_t *boot_application(fake_platform_t *fake)
{
    pomodoro_application_t *application = create_application(fake);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_BOOT) ==
           POMODORO_APPLICATION_OK);
    return application;
}

static void test_boot_without_saved_state_shows_default_focus_ready(void)
{
    fake_platform_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.now_epoch_seconds = 100;
    fake.load_result = POMODORO_APPLICATION_LOAD_EMPTY;
    pomodoro_application_t *application = boot_application(&fake);

    assert(fake.render_count == 1);
    assert(fake.rendered_view.phase == POMODORO_APPLICATION_PHASE_ACTIVE);
    assert(fake.rendered_view.error == POMODORO_APPLICATION_ERROR_NONE);
    assert(fake.rendered_view.mode == POMODORO_MODE_FOCUS);
    assert(fake.rendered_view.timer_state == POMODORO_TIMER_READY);
    assert(fake.rendered_view.remaining_seconds == 25U * 60U);
    assert(fake.rendered_view.set_progress == 0);
    assert(fake.rendered_view.settings.focus_minutes == 25);
    assert(fake.rendered_view.settings.short_break_minutes == 5);
    assert(fake.rendered_view.settings.long_break_minutes == 15);
    assert(fake.rendered_view.settings.daily_goal == 8);
    assert(fake.rendered_view.settings.auto_start_breaks);
    assert(!fake.rendered_view.settings.auto_start_focus);
    assert(fake.rendered_view.settings.volume == 3);
    assert(fake.rendered_view.settings.brightness_percent == 70);
    assert(fake.rendered_view.completion_event == POMODORO_EVENT_NONE);
    assert(fake.save_count == 1);
    pomodoro_application_destroy(application);
}

static void test_start_and_deadline_emit_one_focus_completion(void)
{
    fake_platform_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.now_epoch_seconds = 100;
    fake.load_result = POMODORO_APPLICATION_LOAD_EMPTY;
    pomodoro_application_t *application = boot_application(&fake);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_START) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.timer_state == POMODORO_TIMER_RUNNING);
    assert(fake.rendered_view.remaining_seconds == 25U * 60U);
    assert(fake.save_count == 2);

    fake.now_epoch_seconds = 1600;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_TICK) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.timer_state == POMODORO_TIMER_COMPLETION);
    assert(fake.rendered_view.remaining_seconds == 0);
    assert(fake.rendered_view.set_progress == 1);
    assert(fake.save_count == 3);
    assert(fake.focus_audio_count == 1);
    assert(fake.break_audio_count == 0);
    assert(fake.focus_completion_event_count == 1);
    assert(fake.break_completion_event_count == 0);
    assert(fake.rendered_view.completion_event ==
           POMODORO_EVENT_FOCUS_COMPLETE);
    assert(!fake.rendered_view.daily_goal_reached);

    fake.now_epoch_seconds = 1700;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_TICK) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.mode == POMODORO_MODE_SHORT_BREAK);
    assert(fake.rendered_view.timer_state == POMODORO_TIMER_RUNNING);
    assert(fake.save_count == 4);
    assert(fake.focus_audio_count == 1);
    assert(fake.break_audio_count == 0);
    assert(fake.focus_completion_event_count == 1);
    assert(fake.rendered_view.completion_event == POMODORO_EVENT_NONE);
    pomodoro_application_destroy(application);
}

static void test_daily_goal_celebration_is_exposed_until_completion_resolves(void)
{
    fake_platform_t fake;
    pomodoro_model_t model;
    pomodoro_statistics_store_t statistics;
    memset(&fake, 0, sizeof(fake));
    fake.now_epoch_seconds = 100;
    fake.load_result = POMODORO_APPLICATION_LOAD_LOADED;
    pomodoro_settings_t settings = pomodoro_default_settings();
    settings.daily_goal = 1;
    settings.auto_start_breaks = false;
    assert(pomodoro_init(&model, &settings) == POMODORO_OK);
    pomodoro_statistics_init(&statistics);
    assert(pomodoro_persistence_pack(&fake.persisted, &model, &statistics));

    pomodoro_application_t *application = boot_application(&fake);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_START) ==
           POMODORO_APPLICATION_OK);
    fake.now_epoch_seconds = 1600;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_TICK) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.timer_state == POMODORO_TIMER_COMPLETION);
    assert(fake.rendered_view.daily_goal_reached);

    fake.now_epoch_seconds = 1601;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_TICK) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.timer_state == POMODORO_TIMER_COMPLETION);
    assert(fake.rendered_view.daily_goal_reached);

    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_START_NEXT) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.mode == POMODORO_MODE_SHORT_BREAK);
    assert(!fake.rendered_view.daily_goal_reached);
    pomodoro_application_destroy(application);
}

static void test_save_failure_exposes_error_and_current_timer_state(void)
{
    fake_platform_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.now_epoch_seconds = 100;
    fake.load_result = POMODORO_APPLICATION_LOAD_EMPTY;
    pomodoro_application_t *application = boot_application(&fake);
    fake.fail_save = true;

    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_START) ==
           POMODORO_APPLICATION_ERR_PLATFORM);
    assert(fake.save_count == 2);
    assert(fake.rendered_view.phase ==
           POMODORO_APPLICATION_PHASE_PLATFORM_ERROR);
    assert(fake.rendered_view.error ==
           POMODORO_APPLICATION_ERROR_STORAGE_SAVE);
    assert(fake.rendered_view.mode == POMODORO_MODE_FOCUS);
    assert(fake.rendered_view.timer_state == POMODORO_TIMER_RUNNING);
    assert(fake.rendered_view.remaining_seconds == 25U * 60U);
    pomodoro_application_destroy(application);
}

static void test_boot_platform_failures_are_visible(void)
{
    fake_platform_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.fail_read_time = true;
    pomodoro_application_platform_t platform = fake_platform_services(&fake);
    pomodoro_application_t *application = NULL;

    assert(pomodoro_application_create(&application, &platform) ==
           POMODORO_APPLICATION_OK);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_BOOT) ==
           POMODORO_APPLICATION_ERR_PLATFORM);
    assert(fake.render_count == 1);
    assert(fake.rendered_view.phase ==
           POMODORO_APPLICATION_PHASE_PLATFORM_ERROR);
    assert(fake.rendered_view.error == POMODORO_APPLICATION_ERROR_CLOCK);
    pomodoro_application_destroy(application);

    memset(&fake, 0, sizeof(fake));
    fake.now_epoch_seconds = 100;
    fake.load_result = POMODORO_APPLICATION_LOAD_ERROR;
    platform = fake_platform_services(&fake);
    application = NULL;
    assert(pomodoro_application_create(&application, &platform) ==
           POMODORO_APPLICATION_OK);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_BOOT) ==
           POMODORO_APPLICATION_ERR_PLATFORM);
    assert(fake.render_count == 1);
    assert(fake.rendered_view.phase ==
           POMODORO_APPLICATION_PHASE_PLATFORM_ERROR);
    assert(fake.rendered_view.error ==
           POMODORO_APPLICATION_ERROR_STORAGE_LOAD);
    pomodoro_application_destroy(application);
}

static void test_boot_does_not_overwrite_state_when_journal_read_fails(void)
{
    fake_platform_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.now_epoch_seconds = 100U;
    fake_enable_journal(&fake);
    fake.memory_journal.fail_reads = true;
    pomodoro_application_t *application = create_application(&fake);

    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_BOOT) ==
           POMODORO_APPLICATION_ERR_PLATFORM);
    assert(fake.rendered_view.error ==
           POMODORO_APPLICATION_ERROR_STORAGE_LOAD);
    assert(fake.save_count == 0U);
    pomodoro_application_destroy(application);
}

static void test_boot_with_saved_state_uses_persisted_settings(void)
{
    fake_platform_t fake;
    pomodoro_model_t model;
    pomodoro_statistics_store_t statistics;
    memset(&fake, 0, sizeof(fake));
    fake.now_epoch_seconds = 100;
    fake.load_result = POMODORO_APPLICATION_LOAD_LOADED;
    pomodoro_settings_t settings = pomodoro_default_settings();
    settings.daily_goal = 12;
    settings.auto_start_breaks = false;
    assert(pomodoro_init(&model, &settings) == POMODORO_OK);
    pomodoro_statistics_init(&statistics);
    assert(pomodoro_persistence_pack(&fake.persisted, &model, &statistics));

    pomodoro_application_t *application = boot_application(&fake);
    assert(fake.rendered_view.phase == POMODORO_APPLICATION_PHASE_ACTIVE);
    assert(fake.rendered_view.settings.daily_goal == 12);
    assert(!fake.rendered_view.settings.auto_start_breaks);
    assert(fake.rendered_view.mode == POMODORO_MODE_FOCUS);
    assert(fake.rendered_view.timer_state == POMODORO_TIMER_READY);
    pomodoro_application_destroy(application);
}

static void test_display_and_audio_failures_are_visible(void)
{
    fake_platform_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.now_epoch_seconds = 100;
    fake.load_result = POMODORO_APPLICATION_LOAD_EMPTY;
    fake.fail_render = true;
    pomodoro_application_t *application = create_application(&fake);

    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_BOOT) ==
           POMODORO_APPLICATION_ERR_PLATFORM);
    assert(fake.render_count == 2);
    assert(fake.rendered_view.phase ==
           POMODORO_APPLICATION_PHASE_PLATFORM_ERROR);
    assert(fake.rendered_view.error == POMODORO_APPLICATION_ERROR_DISPLAY);
    pomodoro_application_destroy(application);

    memset(&fake, 0, sizeof(fake));
    fake.now_epoch_seconds = 100;
    fake.load_result = POMODORO_APPLICATION_LOAD_EMPTY;
    application = boot_application(&fake);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_START) ==
           POMODORO_APPLICATION_OK);
    fake.fail_focus_audio = true;
    fake.now_epoch_seconds = 1600;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_TICK) ==
           POMODORO_APPLICATION_ERR_PLATFORM);
    assert(fake.focus_completion_event_count == 1);
    assert(fake.focus_audio_count == 1);
    assert(fake.rendered_view.phase ==
           POMODORO_APPLICATION_PHASE_PLATFORM_ERROR);
    assert(fake.rendered_view.error == POMODORO_APPLICATION_ERROR_AUDIO);
    pomodoro_application_destroy(application);
}

static void test_pause_resume_and_confirmed_reset_preserve_remaining_time(void)
{
    fake_platform_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.now_epoch_seconds = 100;
    fake.load_result = POMODORO_APPLICATION_LOAD_EMPTY;
    pomodoro_application_t *application = boot_application(&fake);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_START) ==
           POMODORO_APPLICATION_OK);

    fake.now_epoch_seconds = 400;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_PAUSE) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.timer_state == POMODORO_TIMER_PAUSED);
    assert(fake.rendered_view.remaining_seconds == 20U * 60U);

    fake.now_epoch_seconds = 500;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_RESUME) ==
           POMODORO_APPLICATION_OK);
    fake.now_epoch_seconds = 600;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_PAUSE) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.remaining_seconds == 1100U);

    unsigned saves_before_confirmation = fake.save_count;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_REQUEST_RESET) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.confirmation ==
           POMODORO_APPLICATION_CONFIRM_RESET);
    assert(fake.rendered_view.timer_state == POMODORO_TIMER_PAUSED);
    assert(fake.rendered_view.remaining_seconds == 1100U);
    assert(fake.save_count == saves_before_confirmation);

    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_CANCEL_CONFIRMATION) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.confirmation ==
           POMODORO_APPLICATION_CONFIRM_NONE);
    assert(fake.rendered_view.timer_state == POMODORO_TIMER_PAUSED);

    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_REQUEST_RESET) ==
           POMODORO_APPLICATION_OK);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_CONFIRM_RESET) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.confirmation ==
           POMODORO_APPLICATION_CONFIRM_NONE);
    assert(fake.rendered_view.timer_state == POMODORO_TIMER_READY);
    assert(fake.rendered_view.remaining_seconds == 25U * 60U);
    assert(fake.rendered_view.set_progress == 0U);
    assert(fake.save_count == saves_before_confirmation + 1U);
    pomodoro_application_destroy(application);
}

static void test_pause_at_deadline_settles_completion_once(void)
{
    fake_platform_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.now_epoch_seconds = 100;
    fake.load_result = POMODORO_APPLICATION_LOAD_EMPTY;
    pomodoro_application_t *application = boot_application(&fake);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_START) ==
           POMODORO_APPLICATION_OK);

    fake.now_epoch_seconds = 1600;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_PAUSE) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.timer_state == POMODORO_TIMER_COMPLETION);
    assert(fake.rendered_view.set_progress == 1U);
    assert(fake.focus_completion_event_count == 1U);
    assert(fake.focus_audio_count == 1U);
    assert(fake.save_count == 3U);
    pomodoro_application_destroy(application);
}

static void test_idle_mode_swipes_do_not_wrap(void)
{
    fake_platform_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.now_epoch_seconds = 100;
    fake.load_result = POMODORO_APPLICATION_LOAD_EMPTY;
    pomodoro_application_t *application = boot_application(&fake);

    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_MODE_PREVIOUS) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.mode == POMODORO_MODE_FOCUS);
    assert(fake.rendered_view.mode_boundary_feedback);
    assert(fake.save_count == 1U);

    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_MODE_NEXT) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.mode == POMODORO_MODE_SHORT_BREAK);
    assert(fake.rendered_view.timer_state == POMODORO_TIMER_READY);
    assert(fake.rendered_view.remaining_seconds == 5U * 60U);
    assert(!fake.rendered_view.mode_boundary_feedback);

    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_MODE_NEXT) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.mode == POMODORO_MODE_LONG_BREAK);
    assert(fake.rendered_view.remaining_seconds == 15U * 60U);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_MODE_NEXT) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.mode == POMODORO_MODE_LONG_BREAK);
    assert(fake.rendered_view.mode_boundary_feedback);
    assert(fake.save_count == 3U);
    pomodoro_application_destroy(application);
}

static void test_active_mode_swipe_requires_confirmation_and_aborts(void)
{
    fake_platform_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.now_epoch_seconds = 100;
    fake.load_result = POMODORO_APPLICATION_LOAD_EMPTY;
    pomodoro_application_t *application = boot_application(&fake);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_START) ==
           POMODORO_APPLICATION_OK);

    unsigned saves_before_request = fake.save_count;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_MODE_NEXT) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.confirmation ==
           POMODORO_APPLICATION_CONFIRM_MODE_SWITCH);
    assert(fake.rendered_view.pending_mode == POMODORO_MODE_SHORT_BREAK);
    assert(fake.rendered_view.mode == POMODORO_MODE_FOCUS);
    assert(fake.rendered_view.timer_state == POMODORO_TIMER_RUNNING);
    assert(fake.save_count == saves_before_request);

    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_CANCEL_CONFIRMATION) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.mode == POMODORO_MODE_FOCUS);
    assert(fake.rendered_view.timer_state == POMODORO_TIMER_RUNNING);

    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_MODE_NEXT) ==
           POMODORO_APPLICATION_OK);
    assert(pomodoro_application_dispatch(
               application,
               POMODORO_APPLICATION_ACTION_CONFIRM_MODE_SWITCH) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.confirmation ==
           POMODORO_APPLICATION_CONFIRM_NONE);
    assert(fake.rendered_view.mode == POMODORO_MODE_SHORT_BREAK);
    assert(fake.rendered_view.timer_state == POMODORO_TIMER_READY);
    assert(fake.rendered_view.set_progress == 0U);
    assert(fake.focus_completion_event_count == 0U);
    assert(fake.focus_audio_count == 0U);
    assert(fake.save_count == saves_before_request + 1U);
    pomodoro_application_destroy(application);
}

static void test_completion_can_be_started_now(void)
{
    fake_platform_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.now_epoch_seconds = 100;
    fake.load_result = POMODORO_APPLICATION_LOAD_EMPTY;
    pomodoro_application_t *application = boot_application(&fake);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_START) ==
           POMODORO_APPLICATION_OK);
    fake.now_epoch_seconds = 1600;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_TICK) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.timer_state == POMODORO_TIMER_COMPLETION);
    assert(fake.rendered_view.scheduled_mode == POMODORO_MODE_SHORT_BREAK);
    assert(fake.rendered_view.auto_start_pending);
    assert(fake.rendered_view.auto_start_seconds_remaining == 3U);

    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_START_NEXT) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.mode == POMODORO_MODE_SHORT_BREAK);
    assert(fake.rendered_view.timer_state == POMODORO_TIMER_RUNNING);
    assert(fake.rendered_view.remaining_seconds == 5U * 60U);
    assert(fake.rendered_view.set_progress == 1U);
    pomodoro_application_destroy(application);
}

static void test_cancel_auto_start_enters_scheduled_mode_ready(void)
{
    fake_platform_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.now_epoch_seconds = 100;
    fake.load_result = POMODORO_APPLICATION_LOAD_EMPTY;
    pomodoro_application_t *application = boot_application(&fake);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_START) ==
           POMODORO_APPLICATION_OK);
    fake.now_epoch_seconds = 1600;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_TICK) ==
           POMODORO_APPLICATION_OK);

    unsigned saves_before_cancel = fake.save_count;
    assert(pomodoro_application_dispatch(
               application,
               POMODORO_APPLICATION_ACTION_CANCEL_AUTO_START) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.mode == POMODORO_MODE_SHORT_BREAK);
    assert(fake.rendered_view.timer_state == POMODORO_TIMER_READY);
    assert(!fake.rendered_view.auto_start_pending);
    assert(fake.rendered_view.settings.auto_start_breaks);
    assert(fake.save_count == saves_before_cancel + 1U);
    pomodoro_application_destroy(application);
}

static void test_completion_dismisses_stale_mode_switch_confirmation(void)
{
    fake_platform_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.now_epoch_seconds = 100;
    fake.load_result = POMODORO_APPLICATION_LOAD_EMPTY;
    pomodoro_application_t *application = boot_application(&fake);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_START) ==
           POMODORO_APPLICATION_OK);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_MODE_NEXT) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.confirmation ==
           POMODORO_APPLICATION_CONFIRM_MODE_SWITCH);

    fake.now_epoch_seconds = 1600;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_TICK) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.timer_state == POMODORO_TIMER_COMPLETION);
    assert(fake.rendered_view.confirmation ==
           POMODORO_APPLICATION_CONFIRM_NONE);
    pomodoro_application_destroy(application);
}

static void test_auto_start_begins_scheduled_mode_after_three_seconds(void)
{
    fake_platform_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.now_epoch_seconds = 100;
    fake.load_result = POMODORO_APPLICATION_LOAD_EMPTY;
    pomodoro_application_t *application = boot_application(&fake);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_START) ==
           POMODORO_APPLICATION_OK);
    fake.now_epoch_seconds = 1600;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_TICK) ==
           POMODORO_APPLICATION_OK);

    fake.now_epoch_seconds = 1602;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_TICK) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.timer_state == POMODORO_TIMER_COMPLETION);
    assert(fake.rendered_view.auto_start_seconds_remaining == 1U);
    fake.now_epoch_seconds = 1603;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_TICK) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.mode == POMODORO_MODE_SHORT_BREAK);
    assert(fake.rendered_view.timer_state == POMODORO_TIMER_RUNNING);
    assert(fake.rendered_view.remaining_seconds == 5U * 60U);
    pomodoro_application_destroy(application);
}

static void test_full_set_schedules_three_short_breaks_then_one_long_break(void)
{
    fake_platform_t fake;
    pomodoro_model_t model;
    pomodoro_statistics_store_t statistics;
    memset(&fake, 0, sizeof(fake));
    fake.now_epoch_seconds = 100;
    fake.load_result = POMODORO_APPLICATION_LOAD_LOADED;
    pomodoro_settings_t settings = pomodoro_default_settings();
    settings.focus_minutes = 5;
    settings.short_break_minutes = 1;
    settings.long_break_minutes = 5;
    settings.auto_start_breaks = false;
    settings.auto_start_focus = false;
    assert(pomodoro_init(&model, &settings) == POMODORO_OK);
    pomodoro_statistics_init(&statistics);
    assert(pomodoro_persistence_pack(&fake.persisted, &model, &statistics));
    pomodoro_application_t *application = boot_application(&fake);

    for (uint8_t completed = 1U; completed <= 4U; completed++) {
        assert(pomodoro_application_dispatch(
                   application, POMODORO_APPLICATION_ACTION_START) ==
               POMODORO_APPLICATION_OK);
        fake.now_epoch_seconds += 300U;
        assert(pomodoro_application_dispatch(
                   application, POMODORO_APPLICATION_ACTION_TICK) ==
               POMODORO_APPLICATION_OK);
        assert(fake.rendered_view.timer_state == POMODORO_TIMER_COMPLETION);
        assert(fake.rendered_view.set_progress == completed);
        assert(!fake.rendered_view.auto_start_pending);
        assert(fake.rendered_view.scheduled_mode ==
               (completed == 4U ? POMODORO_MODE_LONG_BREAK
                                : POMODORO_MODE_SHORT_BREAK));
        assert(pomodoro_application_dispatch(
                   application, POMODORO_APPLICATION_ACTION_MODE_NEXT) ==
               POMODORO_APPLICATION_ERR_INVALID_STATE);

        assert(pomodoro_application_dispatch(
                   application, POMODORO_APPLICATION_ACTION_NOT_NOW) ==
               POMODORO_APPLICATION_OK);
        assert(fake.rendered_view.timer_state == POMODORO_TIMER_READY);
        assert(fake.rendered_view.set_progress == completed);
        assert(pomodoro_application_dispatch(
                   application, POMODORO_APPLICATION_ACTION_START) ==
               POMODORO_APPLICATION_OK);
        fake.now_epoch_seconds += completed == 4U ? 300U : 60U;
        assert(pomodoro_application_dispatch(
                   application, POMODORO_APPLICATION_ACTION_TICK) ==
               POMODORO_APPLICATION_OK);
        assert(fake.rendered_view.timer_state == POMODORO_TIMER_COMPLETION);
        assert(fake.rendered_view.set_progress ==
               (completed == 4U ? 0U : completed));
        assert(fake.rendered_view.scheduled_mode == POMODORO_MODE_FOCUS);
        assert(pomodoro_application_dispatch(
                   application, POMODORO_APPLICATION_ACTION_NOT_NOW) ==
               POMODORO_APPLICATION_OK);
        assert(fake.rendered_view.mode == POMODORO_MODE_FOCUS);
    }
    pomodoro_application_destroy(application);
}

static void test_boot_restarts_completion_countdown_without_offline_chaining(void)
{
    fake_platform_t fake;
    pomodoro_model_t model;
    pomodoro_statistics_store_t statistics;
    memset(&fake, 0, sizeof(fake));
    pomodoro_settings_t settings = pomodoro_default_settings();
    assert(pomodoro_init(&model, &settings) == POMODORO_OK);
    assert(pomodoro_start(&model, 100U) == POMODORO_OK);
    assert(pomodoro_tick(&model, 1600U) == POMODORO_EVENT_FOCUS_COMPLETE);
    pomodoro_statistics_init(&statistics);
    assert(pomodoro_persistence_pack(&fake.persisted, &model, &statistics));
    fake.load_result = POMODORO_APPLICATION_LOAD_LOADED;
    fake.now_epoch_seconds = 5000U;

    pomodoro_application_t *application = boot_application(&fake);
    assert(fake.rendered_view.timer_state == POMODORO_TIMER_COMPLETION);
    assert(fake.rendered_view.auto_start_seconds_remaining == 3U);
    fake.now_epoch_seconds = 5002U;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_TICK) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.timer_state == POMODORO_TIMER_COMPLETION);
    fake.now_epoch_seconds = 5003U;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_TICK) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.mode == POMODORO_MODE_SHORT_BREAK);
    assert(fake.rendered_view.timer_state == POMODORO_TIMER_RUNNING);
    pomodoro_application_destroy(application);
}

static void test_reboot_restores_running_timer_from_rtc_deadline(void)
{
    fake_platform_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.now_epoch_seconds = 100U;
    fake_enable_journal(&fake);

    pomodoro_application_t *application = boot_application(&fake);
    assert(fake.save_count == 1U);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_START) ==
           POMODORO_APPLICATION_OK);
    assert(fake.save_count == 2U);
    pomodoro_application_destroy(application);

    fake.now_epoch_seconds = 400U;
    application = boot_application(&fake);
    assert(fake.rendered_view.timer_state == POMODORO_TIMER_RUNNING);
    assert(fake.rendered_view.remaining_seconds == 20U * 60U);
    assert(fake.save_count == 2U);
    pomodoro_application_destroy(application);
}

static void test_reboot_restores_exact_paused_duration(void)
{
    fake_platform_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.now_epoch_seconds = 100U;
    fake_enable_journal(&fake);

    pomodoro_application_t *application = boot_application(&fake);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_START) ==
           POMODORO_APPLICATION_OK);
    fake.now_epoch_seconds = 400U;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_PAUSE) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.remaining_seconds == 20U * 60U);
    unsigned saves_before_reboot = fake.save_count;
    pomodoro_application_destroy(application);

    fake.now_epoch_seconds = 5000U;
    application = boot_application(&fake);
    assert(fake.rendered_view.timer_state == POMODORO_TIMER_PAUSED);
    assert(fake.rendered_view.remaining_seconds == 20U * 60U);
    assert(fake.save_count == saves_before_reboot);
    pomodoro_application_destroy(application);
}

static void test_reboot_settles_expired_timer_once_without_offline_chaining(void)
{
    fake_platform_t fake;
    pomodoro_model_t model;
    pomodoro_statistics_store_t statistics;
    pomodoro_persisted_state_t initial;
    memset(&fake, 0, sizeof(fake));
    fake.now_epoch_seconds = 100U;
    fake_enable_journal(&fake);
    pomodoro_settings_t settings = pomodoro_default_settings();
    settings.daily_goal = 1U;
    assert(pomodoro_init(&model, &settings) == POMODORO_OK);
    pomodoro_statistics_init(&statistics);
    assert(pomodoro_persistence_pack(&initial, &model, &statistics));
    assert(pomodoro_journal_save(&fake.journal, &initial));

    pomodoro_application_t *application = boot_application(&fake);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_START) ==
           POMODORO_APPLICATION_OK);
    pomodoro_application_destroy(application);

    fake.now_epoch_seconds = 1700U;
    application = boot_application(&fake);
    assert(fake.rendered_view.mode == POMODORO_MODE_FOCUS);
    assert(fake.rendered_view.timer_state == POMODORO_TIMER_COMPLETION);
    assert(fake.rendered_view.set_progress == 1U);
    assert(fake.rendered_view.daily_goal_reached);
    assert(fake.rendered_view.completion_event ==
           POMODORO_EVENT_FOCUS_COMPLETE);
    assert(fake.focus_audio_count == 1U);
    unsigned saves_after_settlement = fake.save_count;
    pomodoro_application_destroy(application);

    fake.now_epoch_seconds = 1800U;
    application = boot_application(&fake);
    assert(fake.rendered_view.mode == POMODORO_MODE_FOCUS);
    assert(fake.rendered_view.timer_state == POMODORO_TIMER_COMPLETION);
    assert(fake.rendered_view.set_progress == 1U);
    assert(fake.rendered_view.daily_goal_reached);
    assert(fake.rendered_view.completion_event == POMODORO_EVENT_NONE);
    assert(fake.focus_audio_count == 1U);
    assert(fake.save_count == saves_after_settlement);
    pomodoro_application_destroy(application);
}

static void test_running_ticks_do_not_write_state_once_per_second(void)
{
    fake_platform_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.now_epoch_seconds = 100U;
    fake_enable_journal(&fake);

    pomodoro_application_t *application = boot_application(&fake);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_START) ==
           POMODORO_APPLICATION_OK);
    unsigned saves_after_start = fake.save_count;
    for (uint64_t now = 101U; now < 110U; ++now) {
        fake.now_epoch_seconds = now;
        assert(pomodoro_application_dispatch(
                   application, POMODORO_APPLICATION_ACTION_TICK) ==
               POMODORO_APPLICATION_OK);
    }
    assert(fake.save_count == saves_after_start);
    assert(fake.rendered_view.timer_state == POMODORO_TIMER_RUNNING);
    pomodoro_application_destroy(application);
}

static void test_offline_completion_uses_deadline_statistics_day(void)
{
    fake_platform_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.now_epoch_seconds = 100U;
    fake.local_date = (pomodoro_date_t) {.year = 2026, .month = 8, .day = 15};
    fake.resolved_date = fake.local_date;
    fake.use_resolved_date = true;
    fake_enable_journal(&fake);

    pomodoro_application_t *application = boot_application(&fake);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_START) ==
           POMODORO_APPLICATION_OK);
    pomodoro_application_destroy(application);

    fake.now_epoch_seconds = 2000U;
    fake.local_date = (pomodoro_date_t) {.year = 2026, .month = 8, .day = 16};
    application = boot_application(&fake);
    pomodoro_persisted_state_t recovered;
    assert(fake_load(&fake, &recovered) ==
           POMODORO_APPLICATION_LOAD_LOADED);
    const pomodoro_daily_statistics_t *deadline_day =
        pomodoro_statistics_find(&recovered.statistics,
                                 fake.resolved_date);
    assert(deadline_day != NULL);
    assert(deadline_day->completed_pomodoros == 1U);
    assert(pomodoro_statistics_find(&recovered.statistics,
                                    fake.local_date) == NULL);
    pomodoro_application_destroy(application);
}

static void test_confirmed_settings_save_and_survive_reboot(void)
{
    fake_platform_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.now_epoch_seconds = 100U;
    fake_enable_journal(&fake);
    pomodoro_application_t *application = boot_application(&fake);
    pomodoro_settings_t settings = fake.rendered_view.settings;
    settings.daily_goal = 12U;
    unsigned saves_before_confirmation = fake.save_count;

    assert(pomodoro_application_confirm_settings(application, &settings) ==
           POMODORO_APPLICATION_OK);
    assert(fake.save_count == saves_before_confirmation + 1U);
    assert(fake.rendered_view.settings.daily_goal == 12U);
    pomodoro_application_destroy(application);

    application = boot_application(&fake);
    assert(fake.rendered_view.settings.daily_goal == 12U);
    assert(fake.save_count == saves_before_confirmation + 1U);
    pomodoro_application_destroy(application);
}

static void test_reboot_falls_back_after_torn_latest_transition(void)
{
    fake_platform_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.now_epoch_seconds = 100U;
    fake_enable_journal(&fake);

    pomodoro_application_t *application = boot_application(&fake);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_START) ==
           POMODORO_APPLICATION_OK);
    fake.memory_journal.tear_next_write = true;
    fake.now_epoch_seconds = 400U;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_PAUSE) ==
           POMODORO_APPLICATION_ERR_PLATFORM);
    pomodoro_application_destroy(application);

    fake.now_epoch_seconds = 500U;
    application = boot_application(&fake);
    assert(fake.rendered_view.timer_state == POMODORO_TIMER_RUNNING);
    assert(fake.rendered_view.remaining_seconds == 1100U);
    pomodoro_application_destroy(application);
}

static void test_boot_migrates_and_resaves_supported_legacy_state(void)
{
    fake_platform_t fake;
    pomodoro_model_t model;
    pomodoro_statistics_store_t statistics;
    memset(&fake, 0, sizeof(fake));
    fake.now_epoch_seconds = 100U;
    fake.load_result = POMODORO_APPLICATION_LOAD_LOADED;
    pomodoro_settings_t settings = pomodoro_default_settings();
    settings.daily_goal = 12U;
    assert(pomodoro_init(&model, &settings) == POMODORO_OK);
    model.timer.timer_state = POMODORO_TIMER_PAUSED;
    model.timer.configured_duration_seconds = 25U * 60U;
    model.timer.remaining_seconds = 137U;
    model.timer.set_progress = 3U;
    pomodoro_statistics_init(&statistics);
    statistics.pending_completed_pomodoros = 2U;
    statistics.pending_focus_seconds = 50U * 60U;
    assert(pomodoro_persistence_pack(&fake.persisted, &model, &statistics));
    fake.persisted.version = 0U;
    fake.persisted.settings.version = 0U;
    fake.persisted.timer.version = 0U;
    fake.persisted.statistics.version = 0U;
    fake.persisted.checksum = pomodoro_test_crc32(
        &fake.persisted, offsetof(pomodoro_persisted_state_t, checksum));

    pomodoro_application_t *application = boot_application(&fake);
    assert(fake.rendered_view.settings.daily_goal == 12U);
    assert(fake.rendered_view.timer_state == POMODORO_TIMER_PAUSED);
    assert(fake.rendered_view.remaining_seconds == 137U);
    assert(fake.rendered_view.set_progress == 3U);
    assert(fake.save_count == 1U);
    assert(fake.persisted.version == POMODORO_PERSISTED_STATE_VERSION);
    assert(fake.persisted.settings.version == POMODORO_SETTINGS_VERSION);
    assert(fake.persisted.timer.version == POMODORO_SNAPSHOT_VERSION);
    assert(fake.persisted.statistics.version == POMODORO_STATISTICS_VERSION);
    assert(fake.persisted.statistics.pending_completed_pomodoros == 2U);
    assert(fake.persisted.statistics.pending_focus_seconds == 50U * 60U);
    pomodoro_application_destroy(application);

    application = boot_application(&fake);
    assert(fake.rendered_view.settings.daily_goal == 12U);
    assert(fake.rendered_view.timer_state == POMODORO_TIMER_PAUSED);
    assert(fake.rendered_view.remaining_seconds == 137U);
    assert(fake.rendered_view.set_progress == 3U);
    assert(fake.save_count == 1U);
    pomodoro_application_destroy(application);
}

int main(void)
{
    test_boot_without_saved_state_shows_default_focus_ready();
    test_start_and_deadline_emit_one_focus_completion();
    test_daily_goal_celebration_is_exposed_until_completion_resolves();
    test_save_failure_exposes_error_and_current_timer_state();
    test_boot_platform_failures_are_visible();
    test_boot_does_not_overwrite_state_when_journal_read_fails();
    test_boot_with_saved_state_uses_persisted_settings();
    test_display_and_audio_failures_are_visible();
    test_pause_resume_and_confirmed_reset_preserve_remaining_time();
    test_pause_at_deadline_settles_completion_once();
    test_idle_mode_swipes_do_not_wrap();
    test_active_mode_swipe_requires_confirmation_and_aborts();
    test_completion_can_be_started_now();
    test_cancel_auto_start_enters_scheduled_mode_ready();
    test_completion_dismisses_stale_mode_switch_confirmation();
    test_auto_start_begins_scheduled_mode_after_three_seconds();
    test_full_set_schedules_three_short_breaks_then_one_long_break();
    test_boot_restarts_completion_countdown_without_offline_chaining();
    test_reboot_restores_running_timer_from_rtc_deadline();
    test_reboot_restores_exact_paused_duration();
    test_reboot_settles_expired_timer_once_without_offline_chaining();
    test_running_ticks_do_not_write_state_once_per_second();
    test_offline_completion_uses_deadline_statistics_day();
    test_confirmed_settings_save_and_survive_reboot();
    test_reboot_falls_back_after_torn_latest_transition();
    test_boot_migrates_and_resaves_supported_legacy_state();
    puts("pomodoro_application_test: all tests passed");
    return 0;
}
