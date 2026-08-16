#include "pomodoro_application.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint64_t now_epoch_seconds;
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

static bool fake_read_time(void *context, pomodoro_application_time_t *time)
{
    fake_platform_t *fake = context;
    if (fake->fail_read_time) {
        return false;
    }
    *time = (pomodoro_application_time_t) {
        .epoch_seconds = fake->now_epoch_seconds,
        .rtc_is_valid = true,
        .local_date = {.year = 2026, .month = 8, .day = 15},
    };
    return true;
}

static pomodoro_application_load_result_t fake_load(
    void *context, pomodoro_persisted_state_t *state)
{
    fake_platform_t *fake = context;
    if (fake->load_result == POMODORO_APPLICATION_LOAD_LOADED) {
        *state = fake->persisted;
    }
    return fake->load_result;
}

static bool fake_save(void *context,
                      const pomodoro_persisted_state_t *state)
{
    fake_platform_t *fake = context;
    (void) state;
    fake->save_count++;
    return !fake->fail_save;
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
    assert(fake.save_count == 0);
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
    assert(fake.save_count == 1);

    fake.now_epoch_seconds = 1600;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_TICK) ==
           POMODORO_APPLICATION_OK);
    assert(fake.rendered_view.timer_state == POMODORO_TIMER_COMPLETION);
    assert(fake.rendered_view.remaining_seconds == 0);
    assert(fake.rendered_view.set_progress == 1);
    assert(fake.save_count == 2);
    assert(fake.focus_audio_count == 1);
    assert(fake.break_audio_count == 0);
    assert(fake.focus_completion_event_count == 1);
    assert(fake.break_completion_event_count == 0);
    assert(fake.rendered_view.completion_event ==
           POMODORO_EVENT_FOCUS_COMPLETE);

    fake.now_epoch_seconds = 1700;
    assert(pomodoro_application_dispatch(
               application, POMODORO_APPLICATION_ACTION_TICK) ==
           POMODORO_APPLICATION_OK);
    assert(fake.save_count == 2);
    assert(fake.focus_audio_count == 1);
    assert(fake.break_audio_count == 0);
    assert(fake.focus_completion_event_count == 1);
    assert(fake.rendered_view.completion_event == POMODORO_EVENT_NONE);
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
    assert(fake.save_count == 1);
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

int main(void)
{
    test_boot_without_saved_state_shows_default_focus_ready();
    test_start_and_deadline_emit_one_focus_completion();
    test_save_failure_exposes_error_and_current_timer_state();
    test_boot_platform_failures_are_visible();
    test_boot_with_saved_state_uses_persisted_settings();
    test_display_and_audio_failures_are_visible();
    puts("pomodoro_application_test: all tests passed");
    return 0;
}
