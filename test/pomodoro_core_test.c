#include "pomodoro_core.h"

#include <assert.h>
#include <stdio.h>

static pomodoro_model_t fresh_model(void)
{
    pomodoro_model_t model;
    pomodoro_settings_t settings = pomodoro_default_settings();
    settings.focus_minutes = 1;
    settings.short_break_minutes = 1;
    settings.long_break_minutes = 1;
    pomodoro_init(&model, &settings);
    return model;
}

static void test_focus_completes_once(void)
{
    pomodoro_model_t model = fresh_model();

    assert(pomodoro_start(&model, 100) == POMODORO_OK);
    assert(pomodoro_tick(&model, 159) == POMODORO_EVENT_NONE);
    assert(pomodoro_tick(&model, 160) == POMODORO_EVENT_FOCUS_COMPLETE);
    assert(pomodoro_tick(&model, 200) == POMODORO_EVENT_NONE);
    assert(model.today_completed_pomodoros == 1);
    assert(model.today_completed_focus_seconds == 60);
    assert(model.timer.set_progress == 1);
}

static void test_pause_resume_excludes_paused_time(void)
{
    pomodoro_model_t model = fresh_model();

    assert(pomodoro_start(&model, 100) == POMODORO_OK);
    assert(pomodoro_pause(&model, 120) == POMODORO_OK);
    assert(model.timer.remaining_seconds == 40);
    assert(pomodoro_tick(&model, 500) == POMODORO_EVENT_NONE);
    assert(pomodoro_resume(&model, 500) == POMODORO_OK);
    assert(pomodoro_tick(&model, 539) == POMODORO_EVENT_NONE);
    assert(pomodoro_tick(&model, 540) == POMODORO_EVENT_FOCUS_COMPLETE);
}

static void test_abort_does_not_count(void)
{
    pomodoro_model_t model = fresh_model();

    assert(pomodoro_start(&model, 100) == POMODORO_OK);
    assert(pomodoro_request_mode(&model, POMODORO_MODE_SHORT_BREAK) ==
           POMODORO_ERR_CONFIRMATION_REQUIRED);
    assert(pomodoro_confirm_mode_switch(&model, POMODORO_MODE_SHORT_BREAK) ==
           POMODORO_OK);
    assert(model.today_completed_pomodoros == 0);
    assert(model.timer.set_progress == 0);
    assert(model.timer.timer_state == POMODORO_TIMER_READY);
}

static void complete_focus(pomodoro_model_t *model, uint64_t start)
{
    assert(pomodoro_start(model, start) == POMODORO_OK);
    assert(pomodoro_tick(model, start + 60) == POMODORO_EVENT_FOCUS_COMPLETE);
}

static void test_fourth_focus_schedules_long_break(void)
{
    pomodoro_model_t model = fresh_model();

    for (unsigned i = 0; i < 4; ++i) {
        complete_focus(&model, 100 + i * 200);
        assert(pomodoro_finish_completion(&model) == POMODORO_OK);
        if (i < 3) {
            assert(model.timer.mode == POMODORO_MODE_SHORT_BREAK);
            assert(pomodoro_request_mode(&model, POMODORO_MODE_FOCUS) == POMODORO_OK);
        }
    }

    assert(model.timer.set_progress == 4);
    assert(model.timer.mode == POMODORO_MODE_LONG_BREAK);
}

static void test_long_break_resets_set(void)
{
    pomodoro_model_t model = fresh_model();
    model.timer.set_progress = 3;

    assert(pomodoro_request_mode(&model, POMODORO_MODE_LONG_BREAK) == POMODORO_OK);
    assert(pomodoro_start(&model, 10) == POMODORO_OK);
    assert(pomodoro_tick(&model, 70) == POMODORO_EVENT_BREAK_COMPLETE);
    assert(model.timer.set_progress == 0);
}

static void test_skipping_long_break_resets_on_next_focus_start(void)
{
    pomodoro_model_t model = fresh_model();
    model.timer.set_progress = 4;

    assert(pomodoro_request_mode(&model, POMODORO_MODE_FOCUS) == POMODORO_OK);
    assert(model.timer.set_progress == 4);
    assert(pomodoro_start(&model, 10) == POMODORO_OK);
    assert(model.timer.set_progress == 0);
}

static void test_restore_settles_expired_timer_exactly_once(void)
{
    pomodoro_model_t before = fresh_model();
    pomodoro_model_t after = fresh_model();
    pomodoro_event_t event = POMODORO_EVENT_NONE;

    assert(pomodoro_start(&before, 100) == POMODORO_OK);
    assert(pomodoro_restore(&after, &before.timer, 200, &event) == POMODORO_OK);
    assert(event == POMODORO_EVENT_FOCUS_COMPLETE);
    assert(after.today_completed_pomodoros == 1);

    pomodoro_snapshot_t settled = after.timer;
    pomodoro_model_t restored_again = fresh_model();
    assert(pomodoro_restore(&restored_again, &settled, 300, &event) == POMODORO_OK);
    assert(event == POMODORO_EVENT_NONE);
    assert(restored_again.today_completed_pomodoros == 0);
}

int main(void)
{
    test_focus_completes_once();
    test_pause_resume_excludes_paused_time();
    test_abort_does_not_count();
    test_fourth_focus_schedules_long_break();
    test_long_break_resets_set();
    test_skipping_long_break_resets_on_next_focus_start();
    test_restore_settles_expired_timer_exactly_once();
    puts("pomodoro_core_test: all tests passed");
    return 0;
}
