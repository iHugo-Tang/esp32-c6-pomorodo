#include "pomodoro_core.h"
#include "pomodoro_statistics.h"
#include "pomodoro_persistence.h"
#include "pomodoro_test_support.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static pomodoro_model_t fresh_model(void)
{
    pomodoro_model_t model;
    pomodoro_settings_t settings = pomodoro_default_settings();
    settings.focus_minutes = 5;
    settings.short_break_minutes = 1;
    settings.long_break_minutes = 5;
    assert(pomodoro_init(&model, &settings) == POMODORO_OK);
    return model;
}

static void test_focus_completes_once(void)
{
    pomodoro_model_t model = fresh_model();

    assert(pomodoro_start(&model, 100) == POMODORO_OK);
    assert(pomodoro_tick(&model, 399) == POMODORO_EVENT_NONE);
    assert(pomodoro_tick(&model, 400) == POMODORO_EVENT_FOCUS_COMPLETE);
    assert(pomodoro_tick(&model, 200) == POMODORO_EVENT_NONE);
    assert(model.timer.last_completion.configured_duration_seconds == 300);
    assert(model.timer.set_progress == 1);
}

static void test_pause_resume_excludes_paused_time(void)
{
    pomodoro_model_t model = fresh_model();

    assert(pomodoro_start(&model, 100) == POMODORO_OK);
    assert(pomodoro_pause(&model, 120, NULL) == POMODORO_OK);
    assert(model.timer.remaining_seconds == 280);
    assert(pomodoro_tick(&model, 500) == POMODORO_EVENT_NONE);
    assert(pomodoro_resume(&model, 500) == POMODORO_OK);
    assert(pomodoro_tick(&model, 779) == POMODORO_EVENT_NONE);
    assert(pomodoro_tick(&model, 780) == POMODORO_EVENT_FOCUS_COMPLETE);
}

static void test_abort_does_not_count(void)
{
    pomodoro_model_t model = fresh_model();

    assert(pomodoro_start(&model, 100) == POMODORO_OK);
    assert(pomodoro_request_mode(&model, POMODORO_MODE_SHORT_BREAK) ==
           POMODORO_ERR_CONFIRMATION_REQUIRED);
    assert(pomodoro_confirm_mode_switch(&model, POMODORO_MODE_SHORT_BREAK) ==
           POMODORO_OK);
    assert(model.timer.set_progress == 0);
    assert(model.timer.timer_state == POMODORO_TIMER_READY);
}

static void complete_focus(pomodoro_model_t *model, uint64_t start)
{
    assert(pomodoro_start(model, start) == POMODORO_OK);
    assert(pomodoro_tick(model, start + 300) == POMODORO_EVENT_FOCUS_COMPLETE);
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
    assert(pomodoro_tick(&model, 310) == POMODORO_EVENT_BREAK_COMPLETE);
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
    assert(pomodoro_restore(&after, &before.timer, 500, &event) == POMODORO_OK);
    assert(event == POMODORO_EVENT_FOCUS_COMPLETE);
    assert(after.timer.last_completion.settlement_sequence == 1);

    pomodoro_snapshot_t settled = after.timer;
    pomodoro_model_t restored_again = fresh_model();
    assert(pomodoro_restore(&restored_again, &settled, 600, &event) == POMODORO_OK);
    assert(event == POMODORO_EVENT_NONE);
    assert(restored_again.timer.last_completion.settlement_sequence == 1);
}

static void test_pause_at_deadline_settles_immediately(void)
{
    pomodoro_model_t model = fresh_model();

    assert(pomodoro_start(&model, 100) == POMODORO_OK);
    pomodoro_event_t event = POMODORO_EVENT_NONE;
    assert(pomodoro_pause(&model, 400, &event) == POMODORO_OK);
    assert(event == POMODORO_EVENT_FOCUS_COMPLETE);
    assert(model.timer.timer_state == POMODORO_TIMER_COMPLETION);
    assert(model.timer.set_progress == 1);
}

static void test_completion_countdown_restarts_after_reboot(void)
{
    pomodoro_model_t before = fresh_model();
    pomodoro_model_t after = fresh_model();
    pomodoro_event_t event;

    complete_focus(&before, 100);
    assert(pomodoro_auto_start_is_due(&before, 1000));
    assert(pomodoro_restore(&after, &before.timer, 1000, &event) == POMODORO_OK);
    assert(!pomodoro_auto_start_is_due(&after, 1002));
    assert(pomodoro_auto_start_is_due(&after, 1003));
}

static void test_settings_validation(void)
{
    pomodoro_model_t model;
    pomodoro_settings_t settings = pomodoro_default_settings();

    settings.volume = 6;
    assert(pomodoro_init(&model, &settings) == POMODORO_ERR_INVALID_ARGUMENT);
    settings = pomodoro_default_settings();
    settings.focus_minutes = 4;
    assert(pomodoro_init(&model, &settings) == POMODORO_ERR_INVALID_ARGUMENT);

    settings = pomodoro_default_settings();
    assert(pomodoro_init(&model, &settings) == POMODORO_OK);
    model.settings.focus_minutes = 0;
    assert(pomodoro_start(&model, 100) == POMODORO_ERR_INVALID_ARGUMENT);
}

static void test_auto_start_can_be_due_or_cancelled(void)
{
    pomodoro_model_t model = fresh_model();

    complete_focus(&model, 100);
    assert(!pomodoro_auto_start_is_due(&model, 402));
    assert(pomodoro_auto_start_is_due(&model, 403));
    assert(pomodoro_cancel_auto_start(&model) == POMODORO_OK);
    assert(!pomodoro_auto_start_is_due(&model, 500));
    assert(model.settings.auto_start_breaks);
}

static void test_statistics_are_deduplicated_and_pending_is_assigned(void)
{
    pomodoro_statistics_store_t statistics;
    pomodoro_model_t model = fresh_model();
    pomodoro_date_t date = {.year = 2026, .month = 8, .day = 15};

    pomodoro_statistics_init(&statistics);
    complete_focus(&model, 100);
    assert(pomodoro_statistics_apply(&statistics, &model.timer.last_completion,
                                     false, (pomodoro_date_t) {0}));
    assert(statistics.pending_completed_pomodoros == 1);
    assert(!pomodoro_statistics_apply(&statistics, &model.timer.last_completion,
                                      false, (pomodoro_date_t) {0}));
    assert(statistics.pending_completed_pomodoros == 1);
    assert(pomodoro_statistics_assign_pending(&statistics, date));
    const pomodoro_daily_statistics_t *day = pomodoro_statistics_find(&statistics, date);
    assert(day != NULL);
    assert(day->completed_pomodoros == 1);
    assert(day->completed_focus_seconds == 300);
}

static void test_persistence_detects_corruption(void)
{
    pomodoro_model_t model = fresh_model();
    pomodoro_model_t restored;
    pomodoro_statistics_store_t statistics;
    pomodoro_statistics_store_t restored_statistics;
    pomodoro_persisted_state_t persisted;

    pomodoro_statistics_init(&statistics);
    assert(pomodoro_persistence_pack(&persisted, &model, &statistics));
    assert(pomodoro_persistence_unpack(&persisted, &restored,
                                       &restored_statistics, NULL));
    assert(restored.settings.daily_goal == model.settings.daily_goal);

    persisted.timer.remaining_seconds++;
    assert(!pomodoro_persistence_unpack(&persisted, &restored,
                                        &restored_statistics, NULL));
}

static void test_journal_survives_torn_write(void)
{
    pomodoro_test_memory_journal_t memory;
    pomodoro_journal_t journal;
    pomodoro_model_t model = fresh_model();
    pomodoro_statistics_store_t statistics;
    pomodoro_persisted_state_t first;
    pomodoro_persisted_state_t second;
    pomodoro_persisted_state_t loaded;

    memset(&memory, 0, sizeof(memory));
    memset(&journal, 0, sizeof(journal));
    pomodoro_test_memory_journal_init(&memory, &journal);
    pomodoro_statistics_init(&statistics);
    assert(pomodoro_persistence_pack(&first, &model, &statistics));
    assert(pomodoro_journal_save(&journal, &first));
    assert(pomodoro_journal_load(&journal, &loaded) ==
           POMODORO_JOURNAL_READ_LOADED);
    assert(loaded.checksum == first.checksum);

    model.settings.daily_goal = 12;
    assert(pomodoro_persistence_pack(&second, &model, &statistics));
    memory.tear_next_write = true;
    assert(!pomodoro_journal_save(&journal, &second));
    assert(pomodoro_journal_load(&journal, &loaded) ==
           POMODORO_JOURNAL_READ_LOADED);
    assert(loaded.checksum == first.checksum);
}

static void test_legacy_v0_state_migrates_to_v1(void)
{
    pomodoro_model_t model = fresh_model();
    pomodoro_model_t restored;
    pomodoro_statistics_store_t statistics;
    pomodoro_statistics_store_t restored_statistics;
    pomodoro_persisted_state_t legacy;

    pomodoro_statistics_init(&statistics);
    assert(pomodoro_persistence_pack(&legacy, &model, &statistics));
    legacy.version = 0;
    legacy.settings.version = 0;
    legacy.timer.version = 0;
    legacy.statistics.version = 0;
    legacy.checksum = pomodoro_test_crc32(
        &legacy, offsetof(pomodoro_persisted_state_t, checksum));

    bool was_migrated = false;
    assert(pomodoro_persistence_unpack(&legacy, &restored,
                                       &restored_statistics, &was_migrated));
    assert(was_migrated);
    assert(legacy.version == POMODORO_PERSISTED_STATE_VERSION);
    assert(legacy.settings.version == POMODORO_SETTINGS_VERSION);
    assert(legacy.timer.version == POMODORO_SNAPSHOT_VERSION);
    assert(legacy.statistics.version == POMODORO_STATISTICS_VERSION);
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
    test_pause_at_deadline_settles_immediately();
    test_completion_countdown_restarts_after_reboot();
    test_settings_validation();
    test_auto_start_can_be_due_or_cancelled();
    test_statistics_are_deduplicated_and_pending_is_assigned();
    test_persistence_detects_corruption();
    test_journal_survives_torn_write();
    test_legacy_v0_state_migrates_to_v1();
    puts("pomodoro_core_test: all tests passed");
    return 0;
}
