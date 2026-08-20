#include "pomodoro_application.h"
#include "pomodoro_settings_editor.h"
#include "pomodoro_test_support.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    pomodoro_test_memory_journal_t memory_journal;
    pomodoro_journal_t journal;
    bool use_journal;
    uint64_t now_wall_time_seconds;
    pomodoro_date_t local_date;
    pomodoro_date_t resolved_date;
    bool use_resolved_date;
    bool resolve_date_from_wall_time;
    bool rtc_is_invalid;
    bool fail_write_time;
    unsigned write_time_count;
    uint64_t written_wall_time_seconds;
    unsigned brightness_count;
    uint8_t brightness_percent;
    pomodoro_application_device_status_t device_status;
    unsigned display_awake_count;
    bool display_awake;
    bool fail_display_awake;
    unsigned power_off_count;
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
        .wall_time_seconds = fake->now_wall_time_seconds,
        .rtc_is_valid = !fake->rtc_is_invalid,
        .local_date = pomodoro_date_is_valid(fake->local_date)
            ? fake->local_date
            : (pomodoro_date_t) {.year = 2026, .month = 8, .day = 15},
    };
    return true;
}

static bool fake_resolve_local_date(void *context, uint64_t wall_time_seconds,
                                    pomodoro_date_t *date)
{
    fake_platform_t *fake = context;
    (void) wall_time_seconds;
    if (date == NULL) {
        return false;
    }
    if (fake->resolve_date_from_wall_time) {
        return pomodoro_local_date_from_wall_time(wall_time_seconds, date);
    }
    *date = fake->use_resolved_date
        ? fake->resolved_date
        : (pomodoro_date_is_valid(fake->local_date)
               ? fake->local_date
               : (pomodoro_date_t) {.year = 2026, .month = 8, .day = 15});
    return pomodoro_date_is_valid(*date);
}

static bool fake_write_time(void *context, uint64_t wall_time_seconds)
{
    fake_platform_t *fake = context;
    fake->write_time_count++;
    fake->written_wall_time_seconds = wall_time_seconds;
    if (fake->fail_write_time) {
        return false;
    }
    fake->now_wall_time_seconds = wall_time_seconds;
    fake->rtc_is_invalid = false;
    return true;
}

static bool fake_set_display_brightness_percent(void *context, uint8_t percent)
{
    fake_platform_t *fake = context;
    fake->brightness_count++;
    fake->brightness_percent = percent;
    return true;
}

static bool fake_read_device_status(
    void *context, pomodoro_application_device_status_t *status)
{
    fake_platform_t *fake = context;
    *status = fake->device_status;
    return true;
}

static bool fake_set_display_awake(void *context, bool awake)
{
    fake_platform_t *fake = context;
    fake->display_awake_count++;
    fake->display_awake = awake;
    return !fake->fail_display_awake;
}

static bool fake_power_off(void *context)
{
    fake_platform_t *fake = context;
    fake->power_off_count++;
    return true;
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
        .write_time = fake_write_time,
        .set_display_brightness_percent =
            fake_set_display_brightness_percent,
        .read_device_status = fake_read_device_status,
        .set_display_awake = fake_set_display_awake,
        .power_off = fake_power_off,
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
    fake.now_wall_time_seconds = 100;
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
    fake.now_wall_time_seconds = 100;
    fake.load_result = POMODORO_APPLICATION_LOAD_EMPTY;
    pomodoro_application_t *application = boot_application(&fake);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_START) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.timer_state == POMODORO_TIMER_RUNNING);
    assert(fake.rendered_view.remaining_seconds == 25U * 60U);
    assert(fake.save_count == 2);

    fake.now_wall_time_seconds = 1600;
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

    fake.now_wall_time_seconds = 1700;
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
    fake.now_wall_time_seconds = 100;
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
    fake.now_wall_time_seconds = 1600;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_TICK) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.timer_state == POMODORO_TIMER_COMPLETION);
    assert(fake.rendered_view.daily_goal_reached);

    fake.now_wall_time_seconds = 1601;
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
    fake.now_wall_time_seconds = 100;
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
    fake.now_wall_time_seconds = 100;
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
    fake.now_wall_time_seconds = 100U;
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

static void test_invalid_rtc_boot_requires_non_skippable_time_setup(void)
{
    fake_platform_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.now_wall_time_seconds = 100U;
    fake.rtc_is_invalid = true;
    fake_enable_journal(&fake);
    pomodoro_application_t *application = boot_application(&fake);

    assert(fake.rendered_view.phase == POMODORO_APPLICATION_PHASE_TIME_SETUP);
    assert(!fake.rendered_view.rtc_is_valid);
    assert(!fake.rendered_view.time_setup_can_cancel);
    assert(fake.save_count == 1U);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_START) ==
           POMODORO_APPLICATION_ERR_INVALID_STATE);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_BACK) ==
           POMODORO_APPLICATION_ERR_INVALID_STATE);
    pomodoro_application_destroy(application);
}

static void test_manual_time_validates_writes_local_time_and_enters_main(void)
{
    fake_platform_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.now_wall_time_seconds = 100U;
    fake.rtc_is_invalid = true;
    fake_enable_journal(&fake);
    pomodoro_application_t *application = boot_application(&fake);
    pomodoro_application_manual_time_t manual = {
        .local_date = {.year = 2026, .month = 2, .day = 29},
        .hour = 14U,
        .minute = 30U,
    };

    assert(pomodoro_application_confirm_manual_time(application, &manual) ==
           POMODORO_APPLICATION_ERR_INVALID_ARGUMENT);
    assert(fake.write_time_count == 0U);

    manual.local_date = (pomodoro_date_t) {
        .year = 2019, .month = 12, .day = 31,
    };
    assert(pomodoro_application_confirm_manual_time(application, &manual) ==
           POMODORO_APPLICATION_ERR_INVALID_ARGUMENT);
    manual.local_date.year = 2070U;
    assert(pomodoro_application_confirm_manual_time(application, &manual) ==
           POMODORO_APPLICATION_ERR_INVALID_ARGUMENT);
    assert(fake.write_time_count == 0U);

    manual.local_date = (pomodoro_date_t) {
        .year = 2026, .month = 8, .day = 15,
    };
    assert(pomodoro_application_confirm_manual_time(application, &manual) ==
           POMODORO_APPLICATION_OK);
    assert(fake.write_time_count == 1U);
    uint64_t expected;
    assert(pomodoro_local_datetime_to_wall_time(
        manual.local_date, manual.hour, manual.minute, &expected));
    assert(fake.written_wall_time_seconds == expected);
    assert(fake.rendered_view.phase == POMODORO_APPLICATION_PHASE_ACTIVE);
    assert(fake.rendered_view.rtc_is_valid);
    pomodoro_application_destroy(application);
}

static void test_time_correction_preserves_running_remaining_duration(void)
{
    fake_platform_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.now_wall_time_seconds = 100U;
    fake_enable_journal(&fake);
    pomodoro_application_t *application = boot_application(&fake);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_START) ==
           POMODORO_APPLICATION_OK);
    fake.now_wall_time_seconds = 400U;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_TICK) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.remaining_seconds == 20U * 60U);

    pomodoro_application_manual_time_t manual = {
        .local_date = {.year = 2026, .month = 8, .day = 15},
        .hour = 14U,
        .minute = 30U,
    };
    assert(pomodoro_application_confirm_manual_time(application, &manual) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.timer_state == POMODORO_TIMER_RUNNING);
    assert(fake.rendered_view.remaining_seconds == 20U * 60U);
    pomodoro_application_destroy(application);
}

static void test_rtc_failure_uses_pending_statistics_until_time_recovers(void)
{
    fake_platform_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.now_wall_time_seconds = 100U;
    fake_enable_journal(&fake);
    pomodoro_application_t *application = boot_application(&fake);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_START) ==
           POMODORO_APPLICATION_OK);

    fake.rtc_is_invalid = true;
    fake.now_wall_time_seconds = 1600U;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_TICK) ==
           POMODORO_APPLICATION_OK);
    pomodoro_persisted_state_t recovered;
    assert(fake_load(&fake, &recovered) ==
           POMODORO_APPLICATION_LOAD_LOADED);
    assert(recovered.statistics.pending_completed_pomodoros == 1U);
    assert(recovered.statistics.count == 0U);
    assert(!fake.rendered_view.rtc_is_valid);

    fake.rtc_is_invalid = false;
    fake.now_wall_time_seconds = 1601U;
    fake.local_date = (pomodoro_date_t) {.year = 2026, .month = 8, .day = 16};
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_TICK) ==
           POMODORO_APPLICATION_OK);
    assert(fake_load(&fake, &recovered) ==
           POMODORO_APPLICATION_LOAD_LOADED);
    assert(recovered.statistics.pending_completed_pomodoros == 0U);
    const pomodoro_daily_statistics_t *day =
        pomodoro_statistics_find(&recovered.statistics, fake.local_date);
    assert(day != NULL);
    assert(day->completed_pomodoros == 1U);
    assert(fake.rendered_view.rtc_is_valid);
    pomodoro_application_destroy(application);
}

static void test_statistics_view_shows_today_and_trailing_seven_days(void)
{
    fake_platform_t fake;
    pomodoro_model_t model;
    pomodoro_statistics_store_t statistics;
    memset(&fake, 0, sizeof(fake));
    fake.now_wall_time_seconds = 100U;
    fake.load_result = POMODORO_APPLICATION_LOAD_LOADED;
    pomodoro_settings_t settings = pomodoro_default_settings();
    assert(pomodoro_init(&model, &settings) == POMODORO_OK);
    pomodoro_statistics_init(&statistics);
    statistics.count = 2U;
    statistics.days[0] = (pomodoro_daily_statistics_t) {
        .date = {.year = 2026, .month = 8, .day = 10},
        .completed_pomodoros = 4U,
        .completed_focus_seconds = 6000U,
    };
    statistics.days[1] = (pomodoro_daily_statistics_t) {
        .date = {.year = 2026, .month = 8, .day = 15},
        .completed_pomodoros = 10U,
        .completed_focus_seconds = 15000U,
    };
    assert(pomodoro_persistence_pack(&fake.persisted, &model, &statistics));
    pomodoro_application_t *application = boot_application(&fake);

    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_OPEN_STATISTICS) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.page == POMODORO_APPLICATION_PAGE_STATISTICS);
    assert(fake.rendered_view.statistics_available);
    assert(fake.rendered_view.today_completed_pomodoros == 10U);
    assert(fake.rendered_view.today_completed_focus_seconds == 15000U);
    assert(fake.rendered_view.today_goal_percent == 125U);
    assert(fake.rendered_view.today_goal_fill_percent == 100U);
    assert(fake.rendered_view.statistics_chart_scale == 10U);
    assert(fake.rendered_view.trailing_seven_days[1].date.day == 10U);
    assert(fake.rendered_view.trailing_seven_days[1].completed_pomodoros == 4U);
    assert(fake.rendered_view.trailing_seven_days[6].date.day == 15U);
    assert(fake.rendered_view.trailing_seven_days[6].completed_pomodoros == 10U);
    assert(fake.rendered_view.trailing_seven_days[6].is_today);

    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_BACK) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.page == POMODORO_APPLICATION_PAGE_MAIN);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_OPEN_SETTINGS) ==
           POMODORO_APPLICATION_OK);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_OPEN_STATISTICS) ==
           POMODORO_APPLICATION_OK);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_BACK) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.page == POMODORO_APPLICATION_PAGE_SETTINGS);
    pomodoro_application_destroy(application);
}

static void test_statistics_hide_chart_while_rtc_is_invalid(void)
{
    fake_platform_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.now_wall_time_seconds = 100U;
    fake_enable_journal(&fake);
    pomodoro_application_t *application = boot_application(&fake);
    fake.rtc_is_invalid = true;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_TICK) ==
           POMODORO_APPLICATION_OK);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_OPEN_STATISTICS) ==
           POMODORO_APPLICATION_OK);
    assert(!fake.rendered_view.statistics_available);
    assert(fake.rendered_view.statistics_chart_scale ==
           fake.rendered_view.settings.daily_goal);
    pomodoro_application_destroy(application);
}

static void test_main_menu_routes_to_statistics_settings_and_back(void)
{
    fake_platform_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.now_wall_time_seconds = 100U;
    fake_enable_journal(&fake);
    pomodoro_application_t *application = boot_application(&fake);

    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_OPEN_MENU) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.page == POMODORO_APPLICATION_PAGE_MENU);

    unsigned saves_before_clear = fake.save_count;
    assert(pomodoro_application_dispatch(
               application,
               POMODORO_APPLICATION_ACTION_REQUEST_CLEAR_SET_PROGRESS) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.confirmation ==
           POMODORO_APPLICATION_CONFIRM_CLEAR_SET_PROGRESS);
    assert(pomodoro_application_dispatch(
               application,
               POMODORO_APPLICATION_ACTION_CONFIRM_CLEAR_SET_PROGRESS) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.page == POMODORO_APPLICATION_PAGE_MAIN);
    assert(fake.rendered_view.set_progress == 0U);
    assert(fake.save_count == saves_before_clear + 1U);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_OPEN_MENU) ==
           POMODORO_APPLICATION_OK);

    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_OPEN_STATISTICS) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.page == POMODORO_APPLICATION_PAGE_STATISTICS);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_BACK) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.page == POMODORO_APPLICATION_PAGE_MENU);

    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_OPEN_SETTINGS) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.page == POMODORO_APPLICATION_PAGE_SETTINGS);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_BACK) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.page == POMODORO_APPLICATION_PAGE_MENU);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_BACK) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.page == POMODORO_APPLICATION_PAGE_MAIN);

    pomodoro_application_destroy(application);
}

static void test_clear_set_progress_from_completion_preserves_statistics(void)
{
    fake_platform_t fake;
    pomodoro_model_t model;
    pomodoro_statistics_store_t statistics;
    memset(&fake, 0, sizeof(fake));
    fake.now_wall_time_seconds = 100U;
    fake.load_result = POMODORO_APPLICATION_LOAD_LOADED;
    pomodoro_settings_t settings = pomodoro_default_settings();
    settings.auto_start_breaks = false;
    assert(pomodoro_init(&model, &settings) == POMODORO_OK);
    model.timer.set_progress = 3U;
    pomodoro_statistics_init(&statistics);
    assert(pomodoro_persistence_pack(&fake.persisted, &model, &statistics));

    pomodoro_application_t *application = boot_application(&fake);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_START) ==
           POMODORO_APPLICATION_OK);
    fake.now_wall_time_seconds = 1600U;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_TICK) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.set_progress == 4U);
    assert(fake.rendered_view.today_completed_pomodoros == 1U);
    assert(fake.rendered_view.scheduled_mode == POMODORO_MODE_LONG_BREAK);

    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_OPEN_MENU) ==
           POMODORO_APPLICATION_OK);
    assert(pomodoro_application_dispatch(
               application,
               POMODORO_APPLICATION_ACTION_REQUEST_CLEAR_SET_PROGRESS) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.confirmation ==
           POMODORO_APPLICATION_CONFIRM_CLEAR_SET_PROGRESS);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_CANCEL_CONFIRMATION) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.page == POMODORO_APPLICATION_PAGE_MENU);
    assert(fake.rendered_view.set_progress == 4U);

    assert(pomodoro_application_dispatch(
               application,
               POMODORO_APPLICATION_ACTION_REQUEST_CLEAR_SET_PROGRESS) ==
           POMODORO_APPLICATION_OK);
    unsigned saves_before_clear = fake.save_count;
    assert(pomodoro_application_dispatch(
               application,
               POMODORO_APPLICATION_ACTION_CONFIRM_CLEAR_SET_PROGRESS) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.page == POMODORO_APPLICATION_PAGE_MAIN);
    assert(fake.rendered_view.timer_state == POMODORO_TIMER_COMPLETION);
    assert(fake.rendered_view.set_progress == 0U);
    assert(fake.rendered_view.today_completed_pomodoros == 1U);
    assert(fake.rendered_view.scheduled_mode == POMODORO_MODE_SHORT_BREAK);
    assert(fake.save_count == saves_before_clear + 1U);

    pomodoro_application_destroy(application);
    application = boot_application(&fake);
    assert(fake.rendered_view.timer_state == POMODORO_TIMER_COMPLETION);
    assert(fake.rendered_view.set_progress == 0U);
    assert(fake.rendered_view.today_completed_pomodoros == 1U);
    assert(fake.rendered_view.scheduled_mode == POMODORO_MODE_SHORT_BREAK);
    pomodoro_application_destroy(application);
}

static void test_clear_set_progress_preserves_running_focus(void)
{
    fake_platform_t fake;
    pomodoro_model_t model;
    pomodoro_statistics_store_t statistics;
    memset(&fake, 0, sizeof(fake));
    fake.now_wall_time_seconds = 100U;
    fake.load_result = POMODORO_APPLICATION_LOAD_LOADED;
    pomodoro_settings_t settings = pomodoro_default_settings();
    settings.auto_start_breaks = false;
    assert(pomodoro_init(&model, &settings) == POMODORO_OK);
    model.timer.set_progress = 2U;
    pomodoro_statistics_init(&statistics);
    assert(pomodoro_persistence_pack(&fake.persisted, &model, &statistics));

    pomodoro_application_t *application = boot_application(&fake);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_START) ==
           POMODORO_APPLICATION_OK);
    fake.now_wall_time_seconds = 400U;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_TICK) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.remaining_seconds == 1200U);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_OPEN_MENU) ==
           POMODORO_APPLICATION_OK);
    assert(pomodoro_application_dispatch(
               application,
               POMODORO_APPLICATION_ACTION_REQUEST_CLEAR_SET_PROGRESS) ==
           POMODORO_APPLICATION_OK);
    assert(pomodoro_application_dispatch(
               application,
               POMODORO_APPLICATION_ACTION_CONFIRM_CLEAR_SET_PROGRESS) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.timer_state == POMODORO_TIMER_RUNNING);
    assert(fake.rendered_view.remaining_seconds == 1200U);
    assert(fake.rendered_view.set_progress == 0U);

    fake.now_wall_time_seconds = 1600U;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_TICK) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.timer_state == POMODORO_TIMER_COMPLETION);
    assert(fake.rendered_view.set_progress == 1U);
    assert(fake.rendered_view.today_completed_pomodoros == 1U);
    pomodoro_application_destroy(application);
}

static void test_focus_completion_cancels_pending_set_progress_clear(void)
{
    fake_platform_t fake;
    pomodoro_model_t model;
    pomodoro_statistics_store_t statistics;
    memset(&fake, 0, sizeof(fake));
    fake.now_wall_time_seconds = 100U;
    fake.load_result = POMODORO_APPLICATION_LOAD_LOADED;
    pomodoro_settings_t settings = pomodoro_default_settings();
    settings.auto_start_breaks = false;
    assert(pomodoro_init(&model, &settings) == POMODORO_OK);
    model.timer.set_progress = 2U;
    pomodoro_statistics_init(&statistics);
    assert(pomodoro_persistence_pack(&fake.persisted, &model, &statistics));

    pomodoro_application_t *application = boot_application(&fake);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_START) ==
           POMODORO_APPLICATION_OK);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_OPEN_MENU) ==
           POMODORO_APPLICATION_OK);
    assert(pomodoro_application_dispatch(
               application,
               POMODORO_APPLICATION_ACTION_REQUEST_CLEAR_SET_PROGRESS) ==
           POMODORO_APPLICATION_OK);

    fake.now_wall_time_seconds = 1600U;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_TICK) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.page == POMODORO_APPLICATION_PAGE_MAIN);
    assert(fake.rendered_view.confirmation ==
           POMODORO_APPLICATION_CONFIRM_NONE);
    assert(fake.rendered_view.set_progress == 3U);
    assert(pomodoro_application_dispatch(
               application,
               POMODORO_APPLICATION_ACTION_CONFIRM_CLEAR_SET_PROGRESS) ==
           POMODORO_APPLICATION_ERR_INVALID_STATE);
    pomodoro_application_destroy(application);
}

static void test_completion_behind_statistics_returns_to_main(void)
{
    fake_platform_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.now_wall_time_seconds = 100U;
    fake_enable_journal(&fake);
    pomodoro_application_t *application = boot_application(&fake);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_START) ==
           POMODORO_APPLICATION_OK);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_OPEN_STATISTICS) ==
           POMODORO_APPLICATION_OK);
    fake.now_wall_time_seconds = 1600U;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_TICK) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.page == POMODORO_APPLICATION_PAGE_MAIN);
    assert(fake.rendered_view.timer_state == POMODORO_TIMER_COMPLETION);
    pomodoro_application_destroy(application);
}

static void test_numeric_stepper_discards_back_and_saves_confirm(void)
{
    fake_platform_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.now_wall_time_seconds = 100U;
    fake_enable_journal(&fake);
    pomodoro_application_t *application = boot_application(&fake);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_OPEN_SETTINGS) ==
           POMODORO_APPLICATION_OK);
    assert(pomodoro_application_open_numeric_setting(
               application,
               POMODORO_APPLICATION_SETTING_FOCUS_MINUTES) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.numeric_draft_value == 25U);
    assert(fake.rendered_view.numeric_minimum == 5U);
    assert(fake.rendered_view.numeric_maximum == 60U);
    assert(pomodoro_application_adjust_numeric_setting(application, 1) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.numeric_draft_value == 26U);
    unsigned saves_before_back = fake.save_count;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_BACK) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.page == POMODORO_APPLICATION_PAGE_SETTINGS);
    assert(fake.save_count == saves_before_back);

    assert(pomodoro_application_open_numeric_setting(
               application,
               POMODORO_APPLICATION_SETTING_FOCUS_MINUTES) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.numeric_draft_value == 25U);
    assert(pomodoro_application_adjust_numeric_setting(application, 1) ==
           POMODORO_APPLICATION_OK);
    assert(pomodoro_application_confirm_numeric_setting(application) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.page == POMODORO_APPLICATION_PAGE_SETTINGS);
    assert(fake.rendered_view.settings.focus_minutes == 26U);
    assert(fake.rendered_view.remaining_seconds == 26U * 60U);
    assert(fake.save_count == saves_before_back + 1U);
    pomodoro_application_destroy(application);
}

static void test_duration_change_does_not_alter_running_timer(void)
{
    fake_platform_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.now_wall_time_seconds = 100U;
    fake_enable_journal(&fake);
    pomodoro_application_t *application = boot_application(&fake);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_START) ==
           POMODORO_APPLICATION_OK);
    fake.now_wall_time_seconds = 400U;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_TICK) ==
           POMODORO_APPLICATION_OK);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_OPEN_SETTINGS) ==
           POMODORO_APPLICATION_OK);
    assert(pomodoro_application_open_numeric_setting(
               application,
               POMODORO_APPLICATION_SETTING_FOCUS_MINUTES) ==
           POMODORO_APPLICATION_OK);
    assert(pomodoro_application_adjust_numeric_setting(application, 1) ==
           POMODORO_APPLICATION_OK);
    assert(pomodoro_application_confirm_numeric_setting(application) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.settings.focus_minutes == 26U);
    assert(fake.rendered_view.configured_duration_seconds == 25U * 60U);
    assert(fake.rendered_view.remaining_seconds == 20U * 60U);
    pomodoro_application_destroy(application);
}

static void test_switches_and_brightness_save_with_specified_timing(void)
{
    fake_platform_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.now_wall_time_seconds = 100U;
    fake_enable_journal(&fake);
    pomodoro_application_t *application = boot_application(&fake);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_OPEN_SETTINGS) ==
           POMODORO_APPLICATION_OK);
    pomodoro_settings_t settings = fake.rendered_view.settings;
    settings.auto_start_breaks = false;
    unsigned saves_before_switch = fake.save_count;
    assert(pomodoro_application_confirm_settings(application, &settings) ==
           POMODORO_APPLICATION_OK);
    assert(fake.save_count == saves_before_switch + 1U);

    unsigned saves_before_preview = fake.save_count;
    assert(pomodoro_application_preview_brightness(application, 85U) ==
           POMODORO_APPLICATION_OK);
    assert(fake.brightness_percent == 85U);
    assert(fake.rendered_view.brightness_preview_percent == 85U);
    assert(fake.save_count == saves_before_preview);
    assert(pomodoro_application_confirm_brightness(application, 85U) ==
           POMODORO_APPLICATION_OK);
    assert(fake.save_count == saves_before_preview + 1U);
    pomodoro_application_destroy(application);

    application = boot_application(&fake);
    assert(!fake.rendered_view.settings.auto_start_breaks);
    assert(fake.rendered_view.settings.brightness_percent == 85U);
    assert(fake.brightness_percent == 85U);
    pomodoro_application_destroy(application);
}

static void test_brightness_numeric_page_previews_and_saves_on_confirm(void)
{
    fake_platform_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.now_wall_time_seconds = 100U;
    fake_enable_journal(&fake);
    pomodoro_application_t *application = boot_application(&fake);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_OPEN_SETTINGS) ==
           POMODORO_APPLICATION_OK);

    unsigned saves_before_preview = fake.save_count;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_OPEN_BRIGHTNESS) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.page ==
           POMODORO_APPLICATION_PAGE_NUMERIC_STEPPER);
    assert(fake.rendered_view.numeric_setting ==
           POMODORO_APPLICATION_SETTING_BRIGHTNESS_PERCENT);
    assert(fake.rendered_view.numeric_draft_value == 70U);
    assert(fake.rendered_view.numeric_minimum == 10U);
    assert(fake.rendered_view.numeric_maximum == 100U);

    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_NUMERIC_INCREMENT) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.numeric_draft_value == 75U);
    assert(fake.brightness_percent == 75U);
    assert(fake.save_count == saves_before_preview);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_BACK) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.page == POMODORO_APPLICATION_PAGE_SETTINGS);
    assert(fake.brightness_percent == 70U);
    assert(fake.rendered_view.settings.brightness_percent == 70U);
    assert(fake.save_count == saves_before_preview);

    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_OPEN_BRIGHTNESS) ==
           POMODORO_APPLICATION_OK);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_NUMERIC_DECREMENT) ==
           POMODORO_APPLICATION_OK);
    assert(fake.brightness_percent == 65U);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_NUMERIC_CONFIRM) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.page == POMODORO_APPLICATION_PAGE_SETTINGS);
    assert(fake.rendered_view.settings.brightness_percent == 65U);
    assert(fake.save_count == saves_before_preview + 1U);
    pomodoro_application_destroy(application);
}

static void test_idle_brightness_multiplier_saves_and_dims_ready_display(void)
{
    fake_platform_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.now_wall_time_seconds = 100U;
    fake_enable_journal(&fake);
    pomodoro_application_t *application = boot_application(&fake);
    assert(fake.rendered_view.settings.idle_brightness_percent == 20U);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_OPEN_SETTINGS) ==
           POMODORO_APPLICATION_OK);
    assert(pomodoro_application_dispatch(
               application,
               POMODORO_APPLICATION_ACTION_OPEN_IDLE_BRIGHTNESS) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.numeric_setting ==
           POMODORO_APPLICATION_SETTING_IDLE_BRIGHTNESS_PERCENT);
    assert(fake.rendered_view.numeric_draft_value == 20U);
    assert(fake.rendered_view.numeric_minimum == 10U);
    assert(fake.rendered_view.numeric_maximum == 100U);
    assert(fake.rendered_view.numeric_step == 5U);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_NUMERIC_INCREMENT) ==
           POMODORO_APPLICATION_OK);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_NUMERIC_CONFIRM) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.settings.idle_brightness_percent == 25U);
    pomodoro_application_destroy(application);

    application = boot_application(&fake);
    assert(fake.rendered_view.settings.idle_brightness_percent == 25U);
    fake.now_wall_time_seconds += 120U;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_TICK) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.effective_brightness_percent == 17U);
    pomodoro_application_destroy(application);
}

static void test_volume_buttons_and_sound_toggle_persist_last_nonzero_volume(void)
{
    fake_platform_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.now_wall_time_seconds = 100U;
    fake_enable_journal(&fake);
    pomodoro_application_t *application = boot_application(&fake);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_START) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.timer_state == POMODORO_TIMER_RUNNING);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_VOLUME_UP) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.settings.volume == 4U);
    assert(fake.rendered_view.last_nonzero_volume == 4U);
    assert(fake.rendered_view.volume_feedback_visible);
    assert(fake.rendered_view.timer_state == POMODORO_TIMER_RUNNING);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_TOGGLE_SOUND) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.settings.volume == 0U);
    assert(fake.rendered_view.last_nonzero_volume == 4U);
    assert(fake.rendered_view.volume_feedback_visible);
    assert(fake.rendered_view.timer_state == POMODORO_TIMER_RUNNING);
    pomodoro_application_destroy(application);

    application = boot_application(&fake);
    assert(fake.rendered_view.settings.volume == 0U);
    assert(fake.rendered_view.last_nonzero_volume == 4U);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_TOGGLE_SOUND) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.settings.volume == 4U);
    assert(fake.rendered_view.volume_feedback_visible);
    fake.now_wall_time_seconds += 2U;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_TICK) ==
           POMODORO_APPLICATION_OK);
    assert(!fake.rendered_view.volume_feedback_visible);
    pomodoro_application_destroy(application);
}

static void test_battery_status_and_one_shot_warning_do_not_cancel_timer(void)
{
    fake_platform_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.now_wall_time_seconds = 100U;
    fake.device_status = (pomodoro_application_device_status_t) {
        .battery_percent = 14U,
        .low_battery = true,
    };
    fake_enable_journal(&fake);
    pomodoro_application_t *application = boot_application(&fake);
    assert(fake.rendered_view.device_status.battery_percent == 14U);
    assert(fake.rendered_view.low_battery_warning_visible);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_START) ==
           POMODORO_APPLICATION_OK);
    fake.now_wall_time_seconds = 104U;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_TICK) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.timer_state == POMODORO_TIMER_RUNNING);
    assert(!fake.rendered_view.low_battery_warning_visible);
    fake.device_status.low_battery = false;
    fake.now_wall_time_seconds = 105U;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_TICK) ==
           POMODORO_APPLICATION_OK);
    fake.device_status.low_battery = true;
    fake.now_wall_time_seconds = 106U;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_TICK) ==
           POMODORO_APPLICATION_OK);
    assert(!fake.rendered_view.low_battery_warning_visible);
    pomodoro_application_destroy(application);
}

static void test_display_lifecycle_dims_sleeps_and_consumes_wake_touch(void)
{
    fake_platform_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.now_wall_time_seconds = 100U;
    fake_enable_journal(&fake);
    pomodoro_application_t *application = boot_application(&fake);
    fake.now_wall_time_seconds = 220U;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_TICK) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.display_awake);
    assert(fake.rendered_view.effective_brightness_percent == 14U);

    bool consumed = true;
    assert(pomodoro_application_handle_touch(application, &consumed) ==
           POMODORO_APPLICATION_OK);
    assert(!consumed);
    assert(fake.rendered_view.display_awake);
    assert(fake.rendered_view.effective_brightness_percent == 70U);

    fake.now_wall_time_seconds = 339U;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_TICK) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.display_awake);
    assert(fake.rendered_view.effective_brightness_percent == 70U);
    fake.now_wall_time_seconds = 340U;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_TICK) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.display_awake);
    assert(fake.rendered_view.effective_brightness_percent == 14U);
    fake.now_wall_time_seconds = 519U;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_TICK) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.display_awake);
    assert(fake.rendered_view.effective_brightness_percent == 14U);
    fake.now_wall_time_seconds = 520U;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_TICK) ==
           POMODORO_APPLICATION_OK);
    assert(!fake.rendered_view.display_awake);
    assert(!fake.display_awake);

    assert(pomodoro_application_handle_touch(application, &consumed) ==
           POMODORO_APPLICATION_OK);
    assert(consumed);
    assert(fake.rendered_view.display_awake);
    assert(fake.rendered_view.effective_brightness_percent == 70U);
    assert(pomodoro_application_handle_touch(application, &consumed) ==
           POMODORO_APPLICATION_OK);
    assert(!consumed);
    pomodoro_application_destroy(application);
}

static void test_auto_display_off_then_first_pwr_press_wakes_and_resets_idle(void)
{
    fake_platform_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.now_wall_time_seconds = 100U;
    fake_enable_journal(&fake);
    pomodoro_application_t *application = boot_application(&fake);

    fake.now_wall_time_seconds = 400U;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_TICK) ==
           POMODORO_APPLICATION_OK);
    assert(!fake.rendered_view.display_awake);
    assert(!fake.display_awake);
    assert(fake.rendered_view.timer_state == POMODORO_TIMER_READY);

    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_TOGGLE_DISPLAY) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.display_awake);
    assert(fake.display_awake);
    assert(fake.rendered_view.effective_brightness_percent == 70U);
    assert(fake.brightness_percent == 70U);
    assert(fake.rendered_view.timer_state == POMODORO_TIMER_READY);
    assert(fake.rendered_view.remaining_seconds == 25U * 60U);

    fake.now_wall_time_seconds = 519U;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_TICK) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.display_awake);
    assert(fake.rendered_view.effective_brightness_percent == 70U);

    fake.now_wall_time_seconds = 520U;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_TICK) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.display_awake);
    assert(fake.rendered_view.effective_brightness_percent == 14U);
    pomodoro_application_destroy(application);
}

static void test_wake_touch_stays_consumed_when_recovery_fails(void)
{
    fake_platform_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.now_wall_time_seconds = 100U;
    fake_enable_journal(&fake);
    pomodoro_application_t *application = boot_application(&fake);
    fake.now_wall_time_seconds = 400U;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_TICK) ==
           POMODORO_APPLICATION_OK);
    assert(!fake.rendered_view.display_awake);
    assert(!fake.display_awake);

    fake.fail_display_awake = true;
    bool consumed = false;
    assert(pomodoro_application_handle_touch(application, &consumed) ==
           POMODORO_APPLICATION_ERR_PLATFORM);
    assert(consumed);
    pomodoro_application_destroy(application);
}

static void test_running_dims_to_sixty_percent_and_completion_wakes_display(void)
{
    fake_platform_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.now_wall_time_seconds = 100U;
    fake_enable_journal(&fake);
    pomodoro_application_t *application = boot_application(&fake);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_START) ==
           POMODORO_APPLICATION_OK);
    fake.now_wall_time_seconds = 220U;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_TICK) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.effective_brightness_percent == 42U);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_TOGGLE_DISPLAY) ==
           POMODORO_APPLICATION_OK);
    assert(!fake.rendered_view.display_awake);
    fake.now_wall_time_seconds = 1600U;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_TICK) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.timer_state == POMODORO_TIMER_COMPLETION);
    assert(fake.rendered_view.display_awake);
    assert(fake.rendered_view.effective_brightness_percent == 70U);
    pomodoro_application_destroy(application);
}

static void test_power_controls_do_not_change_timer_state(void)
{
    fake_platform_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.now_wall_time_seconds = 100U;
    fake_enable_journal(&fake);
    pomodoro_application_t *application = boot_application(&fake);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_START) ==
           POMODORO_APPLICATION_OK);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_TOGGLE_DISPLAY) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.timer_state == POMODORO_TIMER_RUNNING);
    assert(!fake.rendered_view.display_awake);
    fake.now_wall_time_seconds = 101U;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_TICK) ==
           POMODORO_APPLICATION_OK);
    assert(!fake.rendered_view.display_awake);
    assert(fake.rendered_view.timer_state == POMODORO_TIMER_RUNNING);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_TOGGLE_DISPLAY) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.display_awake);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_POWER_OFF) ==
           POMODORO_APPLICATION_OK);
    assert(fake.power_off_count == 1U);
    pomodoro_application_destroy(application);
}

static void test_boot_with_saved_state_uses_persisted_settings(void)
{
    fake_platform_t fake;
    pomodoro_model_t model;
    pomodoro_statistics_store_t statistics;
    memset(&fake, 0, sizeof(fake));
    fake.now_wall_time_seconds = 100;
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
    fake.now_wall_time_seconds = 100;
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
    fake.now_wall_time_seconds = 100;
    fake.load_result = POMODORO_APPLICATION_LOAD_EMPTY;
    application = boot_application(&fake);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_START) ==
           POMODORO_APPLICATION_OK);
    fake.fail_focus_audio = true;
    fake.now_wall_time_seconds = 1600;
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
    fake.now_wall_time_seconds = 100;
    fake.load_result = POMODORO_APPLICATION_LOAD_EMPTY;
    pomodoro_application_t *application = boot_application(&fake);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_START) ==
           POMODORO_APPLICATION_OK);

    fake.now_wall_time_seconds = 400;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_PAUSE) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.timer_state == POMODORO_TIMER_PAUSED);
    assert(fake.rendered_view.remaining_seconds == 20U * 60U);

    fake.now_wall_time_seconds = 500;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_RESUME) ==
           POMODORO_APPLICATION_OK);
    fake.now_wall_time_seconds = 600;
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
    fake.now_wall_time_seconds = 100;
    fake.load_result = POMODORO_APPLICATION_LOAD_EMPTY;
    pomodoro_application_t *application = boot_application(&fake);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_START) ==
           POMODORO_APPLICATION_OK);

    fake.now_wall_time_seconds = 1600;
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
    fake.now_wall_time_seconds = 100;
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
    fake.now_wall_time_seconds = 100;
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
    fake.now_wall_time_seconds = 100;
    fake.load_result = POMODORO_APPLICATION_LOAD_EMPTY;
    pomodoro_application_t *application = boot_application(&fake);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_START) ==
           POMODORO_APPLICATION_OK);
    fake.now_wall_time_seconds = 1600;
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
    fake.now_wall_time_seconds = 100;
    fake.load_result = POMODORO_APPLICATION_LOAD_EMPTY;
    pomodoro_application_t *application = boot_application(&fake);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_START) ==
           POMODORO_APPLICATION_OK);
    fake.now_wall_time_seconds = 1600;
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
    fake.now_wall_time_seconds = 100;
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

    fake.now_wall_time_seconds = 1600;
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
    fake.now_wall_time_seconds = 100;
    fake.load_result = POMODORO_APPLICATION_LOAD_EMPTY;
    pomodoro_application_t *application = boot_application(&fake);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_START) ==
           POMODORO_APPLICATION_OK);
    fake.now_wall_time_seconds = 1600;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_TICK) ==
           POMODORO_APPLICATION_OK);

    fake.now_wall_time_seconds = 1602;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_TICK) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.timer_state == POMODORO_TIMER_COMPLETION);
    assert(fake.rendered_view.auto_start_seconds_remaining == 1U);
    fake.now_wall_time_seconds = 1603;
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
    fake.now_wall_time_seconds = 100;
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
        fake.now_wall_time_seconds += 300U;
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
        fake.now_wall_time_seconds += completed == 4U ? 300U : 60U;
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
    fake.now_wall_time_seconds = 5000U;

    pomodoro_application_t *application = boot_application(&fake);
    assert(fake.rendered_view.timer_state == POMODORO_TIMER_COMPLETION);
    assert(fake.rendered_view.auto_start_seconds_remaining == 3U);
    fake.now_wall_time_seconds = 5002U;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_TICK) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.timer_state == POMODORO_TIMER_COMPLETION);
    fake.now_wall_time_seconds = 5003U;
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
    fake.now_wall_time_seconds = 100U;
    fake_enable_journal(&fake);

    pomodoro_application_t *application = boot_application(&fake);
    assert(fake.save_count == 1U);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_START) ==
           POMODORO_APPLICATION_OK);
    assert(fake.save_count == 2U);
    pomodoro_application_destroy(application);

    fake.now_wall_time_seconds = 400U;
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
    fake.now_wall_time_seconds = 100U;
    fake_enable_journal(&fake);

    pomodoro_application_t *application = boot_application(&fake);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_START) ==
           POMODORO_APPLICATION_OK);
    fake.now_wall_time_seconds = 400U;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_PAUSE) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.remaining_seconds == 20U * 60U);
    unsigned saves_before_reboot = fake.save_count;
    pomodoro_application_destroy(application);

    fake.now_wall_time_seconds = 5000U;
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
    fake.now_wall_time_seconds = 100U;
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

    fake.now_wall_time_seconds = 1700U;
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

    fake.now_wall_time_seconds = 1800U;
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
    fake.now_wall_time_seconds = 100U;
    fake_enable_journal(&fake);

    pomodoro_application_t *application = boot_application(&fake);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_START) ==
           POMODORO_APPLICATION_OK);
    unsigned saves_after_start = fake.save_count;
    for (uint64_t now = 101U; now < 110U; ++now) {
        fake.now_wall_time_seconds = now;
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
    fake.now_wall_time_seconds = 100U;
    fake.local_date = (pomodoro_date_t) {.year = 2026, .month = 8, .day = 15};
    fake.resolved_date = fake.local_date;
    fake.use_resolved_date = true;
    fake_enable_journal(&fake);

    pomodoro_application_t *application = boot_application(&fake);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_START) ==
           POMODORO_APPLICATION_OK);
    pomodoro_application_destroy(application);

    fake.now_wall_time_seconds = 2000U;
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
    fake.now_wall_time_seconds = 100U;
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
    fake.now_wall_time_seconds = 100U;
    fake_enable_journal(&fake);

    pomodoro_application_t *application = boot_application(&fake);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_START) ==
           POMODORO_APPLICATION_OK);
    fake.memory_journal.tear_next_write = true;
    fake.now_wall_time_seconds = 400U;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_PAUSE) ==
           POMODORO_APPLICATION_ERR_PLATFORM);
    pomodoro_application_destroy(application);

    fake.now_wall_time_seconds = 500U;
    application = boot_application(&fake);
    assert(fake.rendered_view.timer_state == POMODORO_TIMER_RUNNING);
    assert(fake.rendered_view.remaining_seconds == 1100U);
    pomodoro_application_destroy(application);
}

static void test_legacy_state_is_treated_as_empty(void)
{
    fake_platform_t fake;
    pomodoro_model_t model;
    pomodoro_statistics_store_t statistics;
    memset(&fake, 0, sizeof(fake));
    fake.now_wall_time_seconds = 100U;
    fake_enable_journal(&fake);
    pomodoro_settings_t settings = pomodoro_default_settings();
    settings.daily_goal = 12U;
    assert(pomodoro_init(&model, &settings) == POMODORO_OK);
    pomodoro_statistics_init(&statistics);
    pomodoro_persisted_state_t old_format;
    assert(pomodoro_persistence_pack(&old_format, &model, &statistics));
    old_format.version = POMODORO_PERSISTED_STATE_VERSION - 1U;
    old_format.settings.version = POMODORO_SETTINGS_VERSION - 1U;
    pomodoro_journal_slot_t slot = {
        .generation = 1U,
        .state = old_format,
    };
    slot.checksum = pomodoro_test_crc32(
        &slot, offsetof(pomodoro_journal_slot_t, checksum));
    fake.memory_journal.present[0] = true;
    fake.memory_journal.slots[0] = slot;

    pomodoro_application_t *application = boot_application(&fake);
    assert(fake.rendered_view.phase == POMODORO_APPLICATION_PHASE_ACTIVE);
    assert(fake.rendered_view.error == POMODORO_APPLICATION_ERROR_NONE);
    assert(fake.rendered_view.settings.daily_goal == 8U);
    assert(fake.rendered_view.timer_state == POMODORO_TIMER_READY);
    pomodoro_application_destroy(application);
}

static void test_first_setup_volume_overlay_hides_after_tick(void)
{
    fake_platform_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.rtc_is_invalid = true;
    pomodoro_application_t *application = boot_application(&fake);
    assert(fake.rendered_view.phase == POMODORO_APPLICATION_PHASE_TIME_SETUP);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_VOLUME_UP) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.volume_feedback_visible);
    fake.now_wall_time_seconds += 2U;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_TICK) ==
           POMODORO_APPLICATION_OK);
    assert(!fake.rendered_view.volume_feedback_visible);
    pomodoro_application_destroy(application);
}

static void test_second_volume_operation_restarts_overlay_deadline(void)
{
    fake_platform_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.rtc_is_invalid = true;
    pomodoro_application_t *application = boot_application(&fake);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_VOLUME_UP) ==
           POMODORO_APPLICATION_OK);
    fake.now_wall_time_seconds += 1U;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_VOLUME_UP) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.volume_feedback_visible);
    fake.now_wall_time_seconds++;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_TICK) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.volume_feedback_visible);
    fake.now_wall_time_seconds++;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_TICK) ==
           POMODORO_APPLICATION_OK);
    assert(!fake.rendered_view.volume_feedback_visible);
    pomodoro_application_destroy(application);
}

static void test_settings_volume_overlay_hides_after_tick(void)
{
    fake_platform_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.now_wall_time_seconds = 100U;
    pomodoro_application_t *application = boot_application(&fake);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_OPEN_SETTINGS) ==
           POMODORO_APPLICATION_OK);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_VOLUME_UP) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.page == POMODORO_APPLICATION_PAGE_SETTINGS);
    assert(fake.rendered_view.volume_feedback_visible);
    fake.now_wall_time_seconds += 2U;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_TICK) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.page == POMODORO_APPLICATION_PAGE_SETTINGS);
    assert(!fake.rendered_view.volume_feedback_visible);
    pomodoro_application_destroy(application);
}

static void test_settings_time_setup_back_discards_draft(void)
{
    fake_platform_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.now_wall_time_seconds = 100U;
    pomodoro_application_t *application = boot_application(&fake);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_OPEN_SETTINGS) ==
           POMODORO_APPLICATION_OK);
    unsigned saves_before = fake.save_count;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_OPEN_DATE_TIME) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.phase == POMODORO_APPLICATION_PHASE_TIME_SETUP);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_MANUAL_HOUR_UP) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.manual_time_draft.hour == 1U);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_BACK) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.phase == POMODORO_APPLICATION_PHASE_ACTIVE);
    assert(fake.rendered_view.page == POMODORO_APPLICATION_PAGE_SETTINGS);
    assert(fake.save_count == saves_before);
    pomodoro_application_destroy(application);
}

static void test_settings_confirm_returns_to_settings(void)
{
    fake_platform_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.now_wall_time_seconds = 100U;
    pomodoro_application_t *application = boot_application(&fake);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_OPEN_SETTINGS) ==
           POMODORO_APPLICATION_OK);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_OPEN_DATE_TIME) ==
           POMODORO_APPLICATION_OK);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_MANUAL_TIME_CONFIRM) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.phase == POMODORO_APPLICATION_PHASE_ACTIVE);
    assert(fake.rendered_view.page == POMODORO_APPLICATION_PAGE_SETTINGS);
    pomodoro_application_destroy(application);
}

static void test_build_date_fallback_is_valid_with_noontime(void)
{
    fake_platform_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.rtc_is_invalid = true;
    pomodoro_application_t *application = boot_application(&fake);
    assert(fake.rendered_view.phase == POMODORO_APPLICATION_PHASE_TIME_SETUP);
    assert(pomodoro_date_is_valid(
        fake.rendered_view.manual_time_draft.local_date));
    assert(fake.rendered_view.manual_time_draft.hour == 12U);
    assert(fake.rendered_view.manual_time_draft.minute == 0U);
    pomodoro_application_destroy(application);
}

static void test_manual_draft_ranges_are_bounded(void)
{
    pomodoro_application_manual_time_t draft = {
        .local_date = {.year = 2069, .month = 12, .day = 31},
        .hour = 23U,
        .minute = 59U,
    };
    assert(pomodoro_manual_time_adjust(
        &draft, POMODORO_APPLICATION_ACTION_MANUAL_YEAR_UP, 1U));
    assert(draft.local_date.year == 2069U);
    assert(pomodoro_manual_time_adjust(
        &draft, POMODORO_APPLICATION_ACTION_MANUAL_HOUR_UP, 1U));
    assert(draft.hour == 23U);
    assert(pomodoro_manual_time_adjust(
        &draft, POMODORO_APPLICATION_ACTION_MANUAL_MINUTE_UP, 1U));
    assert(draft.minute == 59U);
    assert(pomodoro_manual_time_adjust(
        &draft, POMODORO_APPLICATION_ACTION_MANUAL_MONTH_UP, 1U));
    assert(draft.local_date.month == 12U);
    assert(pomodoro_manual_time_adjust(
        &draft, POMODORO_APPLICATION_ACTION_MANUAL_DAY_UP, 1U));
    assert(draft.local_date.day == 31U);

    draft = (pomodoro_application_manual_time_t) {
        .local_date = {.year = 2020, .month = 1, .day = 1},
        .hour = 0U,
        .minute = 0U,
    };
    assert(pomodoro_manual_time_adjust(
        &draft, POMODORO_APPLICATION_ACTION_MANUAL_YEAR_DOWN, 1U));
    assert(draft.local_date.year == 2020U);
    assert(pomodoro_manual_time_adjust(
        &draft, POMODORO_APPLICATION_ACTION_MANUAL_MONTH_DOWN, 1U));
    assert(draft.local_date.month == 1U);
    assert(pomodoro_manual_time_adjust(
        &draft, POMODORO_APPLICATION_ACTION_MANUAL_DAY_DOWN, 1U));
    assert(draft.local_date.day == 1U);
    assert(pomodoro_manual_time_adjust(
        &draft, POMODORO_APPLICATION_ACTION_MANUAL_HOUR_DOWN, 1U));
    assert(draft.hour == 0U);
    assert(pomodoro_manual_time_adjust(
        &draft, POMODORO_APPLICATION_ACTION_MANUAL_MINUTE_DOWN, 1U));
    assert(draft.minute == 0U);
}

static void test_manual_time_batch_renders_once(void)
{
    fake_platform_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.rtc_is_invalid = true;
    pomodoro_application_t *application = boot_application(&fake);
    unsigned renders_before = fake.render_count;
    assert(pomodoro_application_adjust_manual_time(
               application,
               POMODORO_APPLICATION_ACTION_MANUAL_MINUTE_UP, 59U) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.manual_time_draft.minute == 59U);
    assert(fake.render_count == renders_before + 1U);
    pomodoro_application_destroy(application);
}

static void test_year_month_change_converges_day(void)
{
    pomodoro_application_manual_time_t draft = {
        .local_date = {.year = 2024, .month = 1, .day = 31},
        .hour = 12U,
        .minute = 0U,
    };
    assert(pomodoro_manual_time_adjust(
        &draft, POMODORO_APPLICATION_ACTION_MANUAL_MONTH_UP, 1U));
    assert(draft.local_date.month == 2U);
    assert(draft.local_date.day == 29U);

    draft.local_date = (pomodoro_date_t) {.year = 2023, .month = 1, .day = 31};
    assert(pomodoro_manual_time_adjust(
        &draft, POMODORO_APPLICATION_ACTION_MANUAL_MONTH_UP, 1U));
    assert(draft.local_date.month == 2U);
    assert(draft.local_date.day == 28U);

    draft.local_date = (pomodoro_date_t) {.year = 2024, .month = 4, .day = 31};
    assert(pomodoro_manual_time_adjust(
        &draft, POMODORO_APPLICATION_ACTION_MANUAL_MONTH_UP, 1U));
    assert(draft.local_date.month == 5U);
    assert(draft.local_date.day == 31U);
    draft.local_date = (pomodoro_date_t) {.year = 2024, .month = 4, .day = 30};
    assert(pomodoro_manual_time_adjust(
        &draft, POMODORO_APPLICATION_ACTION_MANUAL_YEAR_UP, 1U));
    assert(draft.local_date.year == 2025U);
    assert(draft.local_date.day == 30U);

    draft.local_date = (pomodoro_date_t) {.year = 2024, .month = 12, .day = 31};
    assert(pomodoro_manual_time_adjust(
        &draft, POMODORO_APPLICATION_ACTION_MANUAL_MONTH_UP, 1U));
    assert(draft.local_date.month == 12U);
    assert(draft.local_date.day == 31U);

    draft.local_date = (pomodoro_date_t) {.year = 2024, .month = 1, .day = 31};
    assert(pomodoro_manual_time_adjust(
        &draft, POMODORO_APPLICATION_ACTION_MANUAL_MONTH_UP, 11U));
    assert(draft.local_date.month == 12U);
    assert(draft.local_date.day == 31U);

    draft.local_date = (pomodoro_date_t) {.year = 2024, .month = 2, .day = 29};
    assert(pomodoro_manual_time_adjust(
        &draft, POMODORO_APPLICATION_ACTION_MANUAL_YEAR_UP, 4U));
    assert(draft.local_date.year == 2028U);
    assert(draft.local_date.day == 29U);
}

static void test_local_midnight_decides_statistics_day(void)
{
    const struct {
        pomodoro_date_t start_date;
        uint8_t start_hour;
        uint8_t start_minute;
        pomodoro_date_t expected_date;
    } cases[] = {
        {
            .start_date = {.year = 2026, .month = 8, .day = 15},
            .start_hour = 0U,
            .start_minute = 0U,
            .expected_date = {.year = 2026, .month = 8, .day = 15},
        },
        {
            .start_date = {.year = 2026, .month = 8, .day = 14},
            .start_hour = 23U,
            .start_minute = 58U,
            .expected_date = {.year = 2026, .month = 8, .day = 15},
        },
    };
    for (size_t index = 0; index < sizeof(cases) / sizeof(cases[0]); index++) {
        fake_platform_t fake;
        pomodoro_model_t model;
        pomodoro_statistics_store_t statistics;
        memset(&fake, 0, sizeof(fake));
        assert(pomodoro_local_datetime_to_wall_time(
            cases[index].start_date, cases[index].start_hour,
            cases[index].start_minute, &fake.now_wall_time_seconds));
        fake.load_result = POMODORO_APPLICATION_LOAD_LOADED;
        fake.resolve_date_from_wall_time = true;
        pomodoro_settings_t settings = pomodoro_default_settings();
        settings.focus_minutes = 5U;
        assert(pomodoro_init(&model, &settings) == POMODORO_OK);
        pomodoro_statistics_init(&statistics);
        assert(pomodoro_persistence_pack(
            &fake.persisted, &model, &statistics));
        pomodoro_application_t *application = boot_application(&fake);
        assert(pomodoro_application_dispatch(
                   application, POMODORO_APPLICATION_ACTION_START) ==
               POMODORO_APPLICATION_OK);
        fake.now_wall_time_seconds += 5U * 60U;
        assert(pomodoro_application_dispatch(
                   application, POMODORO_APPLICATION_ACTION_TICK) ==
               POMODORO_APPLICATION_OK);
        const pomodoro_daily_statistics_t *day = pomodoro_statistics_find(
            &fake.persisted.statistics, cases[index].expected_date);
        assert(day != NULL);
        assert(day->completed_pomodoros == 1U);
        pomodoro_application_destroy(application);
    }
}

static void test_date_time_setup_actions_confirm_manual_draft(void)
{
    fake_platform_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.rtc_is_invalid = true;
    pomodoro_application_t *application = boot_application(&fake);
    assert(fake.rendered_view.phase == POMODORO_APPLICATION_PHASE_TIME_SETUP);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_MANUAL_MINUTE_UP) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.manual_time_draft.minute == 1U);
    uint64_t expected;
    assert(pomodoro_local_datetime_to_wall_time(
        fake.rendered_view.manual_time_draft.local_date, 12U, 1U,
        &expected));
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_MANUAL_TIME_CONFIRM) ==
           POMODORO_APPLICATION_OK);
    assert(fake.written_wall_time_seconds == expected);
    assert(fake.rendered_view.phase == POMODORO_APPLICATION_PHASE_ACTIVE);
    pomodoro_application_destroy(application);
}

static void test_invalid_rtc_boot_rebases_saved_running_remaining_time(void)
{
    fake_platform_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.now_wall_time_seconds = 100U;
    pomodoro_application_t *application = boot_application(&fake);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_START) ==
           POMODORO_APPLICATION_OK);
    pomodoro_application_destroy(application);

    fake.rtc_is_invalid = true;
    fake.now_wall_time_seconds = 0U;
    application = boot_application(&fake);
    assert(fake.rendered_view.phase == POMODORO_APPLICATION_PHASE_TIME_SETUP);
    assert(fake.rendered_view.timer_state == POMODORO_TIMER_RUNNING);
    assert(fake.rendered_view.remaining_seconds == 1500U);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_MANUAL_TIME_CONFIRM) ==
           POMODORO_APPLICATION_OK);
    uint64_t restored_now = fake.written_wall_time_seconds;
    assert(fake.rendered_view.timer_state == POMODORO_TIMER_RUNNING);
    assert(fake.focus_completion_event_count == 0U);
    fake.now_wall_time_seconds = restored_now + 1499U;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_TICK) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.remaining_seconds == 1U);
    fake.now_wall_time_seconds++;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_TICK) ==
           POMODORO_APPLICATION_OK);
    assert(fake.focus_completion_event_count == 1U);
    pomodoro_application_destroy(application);
}

static void test_recovery_tick_assigns_outage_completion_to_recovery_day(void)
{
    fake_platform_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.now_wall_time_seconds = 100U;
    fake.local_date = (pomodoro_date_t) {.year = 2026, .month = 8, .day = 15};
    pomodoro_application_t *application = boot_application(&fake);
    pomodoro_settings_t settings = fake.rendered_view.settings;
    settings.focus_minutes = 5U;
    assert(pomodoro_application_confirm_settings(application, &settings) ==
           POMODORO_APPLICATION_OK);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_START) ==
           POMODORO_APPLICATION_OK);
    fake.rtc_is_invalid = true;
    fake.now_wall_time_seconds = 399U;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_TICK) ==
           POMODORO_APPLICATION_OK);
    fake.rtc_is_invalid = false;
    fake.local_date = (pomodoro_date_t) {.year = 2026, .month = 8, .day = 16};
    fake.now_wall_time_seconds = 400U;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_TICK) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.today_completed_pomodoros == 1U);
    assert(fake.persisted.statistics.pending_completed_pomodoros == 0U);
    assert(pomodoro_statistics_find(
               &fake.persisted.statistics,
               (pomodoro_date_t) {.year = 2026, .month = 8, .day = 15}) ==
           NULL);
    pomodoro_application_destroy(application);
}

static void test_hardware_controls_remain_available_during_time_setup(void)
{
    fake_platform_t fake;
    memset(&fake, 0, sizeof(fake));
    fake.rtc_is_invalid = true;
    pomodoro_application_t *application = boot_application(&fake);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_VOLUME_UP) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.settings.volume == 4U);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_TOGGLE_DISPLAY) ==
           POMODORO_APPLICATION_OK);
    assert(!fake.display_awake);
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_POWER_OFF) ==
           POMODORO_APPLICATION_OK);
    assert(fake.power_off_count == 1U);
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
    test_invalid_rtc_boot_requires_non_skippable_time_setup();
    test_manual_time_validates_writes_local_time_and_enters_main();
    test_time_correction_preserves_running_remaining_duration();
    test_rtc_failure_uses_pending_statistics_until_time_recovers();
    test_statistics_view_shows_today_and_trailing_seven_days();
    test_statistics_hide_chart_while_rtc_is_invalid();
    test_main_menu_routes_to_statistics_settings_and_back();
    test_clear_set_progress_from_completion_preserves_statistics();
    test_clear_set_progress_preserves_running_focus();
    test_focus_completion_cancels_pending_set_progress_clear();
    test_completion_behind_statistics_returns_to_main();
    test_numeric_stepper_discards_back_and_saves_confirm();
    test_duration_change_does_not_alter_running_timer();
    test_switches_and_brightness_save_with_specified_timing();
    test_brightness_numeric_page_previews_and_saves_on_confirm();
    test_idle_brightness_multiplier_saves_and_dims_ready_display();
    test_volume_buttons_and_sound_toggle_persist_last_nonzero_volume();
    test_battery_status_and_one_shot_warning_do_not_cancel_timer();
    test_display_lifecycle_dims_sleeps_and_consumes_wake_touch();
    test_auto_display_off_then_first_pwr_press_wakes_and_resets_idle();
    test_wake_touch_stays_consumed_when_recovery_fails();
    test_running_dims_to_sixty_percent_and_completion_wakes_display();
    test_power_controls_do_not_change_timer_state();
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
    test_legacy_state_is_treated_as_empty();
    test_date_time_setup_actions_confirm_manual_draft();
    test_invalid_rtc_boot_rebases_saved_running_remaining_time();
    test_recovery_tick_assigns_outage_completion_to_recovery_day();
    test_hardware_controls_remain_available_during_time_setup();
    test_first_setup_volume_overlay_hides_after_tick();
    test_second_volume_operation_restarts_overlay_deadline();
    test_settings_volume_overlay_hides_after_tick();
    test_settings_time_setup_back_discards_draft();
    test_settings_confirm_returns_to_settings();
    test_build_date_fallback_is_valid_with_noontime();
    test_manual_draft_ranges_are_bounded();
    test_manual_time_batch_renders_once();
    test_year_month_change_converges_day();
    test_local_midnight_decides_statistics_day();
    puts("pomodoro_application_test: all tests passed");
    return 0;
}
