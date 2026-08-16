#include "pomodoro_core.h"
#include "pomodoro_statistics.h"
#include "pomodoro_persistence.h"
#include "pomodoro_test_support.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint16_t focus_minutes;
    uint16_t short_break_minutes;
    uint16_t long_break_minutes;
    uint8_t daily_goal;
    bool auto_start_breaks;
    bool auto_start_focus;
    uint8_t volume;
    uint8_t brightness_percent;
} legacy_v1_settings_t;

typedef struct {
    uint32_t version;
    legacy_v1_settings_t value;
} legacy_v1_persisted_settings_t;

typedef struct {
    uint32_t version;
    legacy_v1_persisted_settings_t settings;
    pomodoro_snapshot_t timer;
    pomodoro_statistics_store_t statistics;
    uint32_t checksum;
} legacy_v1_persisted_state_t;

typedef struct {
    uint32_t version;
    pomodoro_settings_t value;
    uint8_t last_nonzero_volume;
} legacy_v2_persisted_settings_t;

typedef struct {
    uint32_t version;
    legacy_v2_persisted_settings_t settings;
    pomodoro_snapshot_t timer;
    pomodoro_statistics_store_t statistics;
    uint32_t checksum;
} legacy_v2_persisted_state_t;

typedef struct {
    uint64_t generation;
    legacy_v2_persisted_state_t state;
    uint32_t checksum;
} legacy_v2_journal_slot_t;

_Static_assert(sizeof(legacy_v1_persisted_state_t) <
                   sizeof(pomodoro_persisted_state_t),
               "automatic time-zone data must extend the v1 schema");

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

static void test_clock_rebase_preserves_running_remaining_time(void)
{
    pomodoro_model_t model = fresh_model();
    assert(pomodoro_start(&model, 100U) == POMODORO_OK);
    assert(pomodoro_rebase_running_timer(&model, 220U, 5000U) ==
           POMODORO_OK);
    assert(model.timer.remaining_seconds == 180U);
    assert(model.timer.deadline_epoch_seconds == 5180U);
    assert(pomodoro_tick(&model, 5179U) == POMODORO_EVENT_NONE);
    assert(pomodoro_tick(&model, 5180U) == POMODORO_EVENT_FOCUS_COMPLETE);

    model = fresh_model();
    assert(pomodoro_start(&model, 100U) == POMODORO_OK);
    model.timer.remaining_seconds = 177U;
    assert(pomodoro_rebase_running_timer_from_snapshot(&model, 8000U) ==
           POMODORO_OK);
    assert(model.timer.deadline_epoch_seconds == 8177U);
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

static void test_statistics_evicts_oldest_of_366_days(void)
{
    pomodoro_statistics_store_t statistics;
    pomodoro_statistics_init(&statistics);
    pomodoro_date_t first = {.year = 2025, .month = 1, .day = 1};
    pomodoro_date_t date = first;
    pomodoro_completion_record_t completion = {
        .mode = POMODORO_MODE_FOCUS,
        .configured_duration_seconds = 1500U,
    };
    for (uint16_t index = 0; index < 366U; index++) {
        completion.settlement_sequence = (uint32_t) index + 1U;
        assert(pomodoro_statistics_apply(&statistics, &completion, true,
                                         date));
        if (index < 365U) {
            assert(pomodoro_date_add_days(date, 1, &date));
        }
    }
    assert(statistics.count == POMODORO_STATISTICS_CAPACITY);
    assert(pomodoro_statistics_find(&statistics, first) == NULL);
    assert(pomodoro_statistics_find(&statistics, date) != NULL);
}

static void test_local_time_conversion_uses_fixed_offset(void)
{
    uint64_t utc;
    pomodoro_date_t local = {.year = 2026, .month = 8, .day = 15};
    assert(pomodoro_local_datetime_to_utc(local, 0U, 30U, -24, &utc));
    pomodoro_date_t resolved;
    assert(pomodoro_local_date_from_utc(utc, -24, &resolved));
    assert(resolved.year == 2026U && resolved.month == 8U &&
           resolved.day == 15U);
    assert(pomodoro_local_date_from_utc(utc, 0, &resolved));
    assert(resolved.day == 15U);
}

static void test_time_zone_schedule_applies_transitions_and_expiry(void)
{
    pomodoro_time_zone_schedule_t schedule;
    memset(&schedule, 0, sizeof(schedule));
    schedule.automatic = true;
    strcpy(schedule.iana_name, "America/Edmonton");
    schedule.transition_count = 3U;
    schedule.expires_utc_epoch_seconds = 4000000U;
    schedule.transitions[0] = (pomodoro_utc_offset_transition_t) {
        .effective_utc_epoch_seconds = 0U,
        .offset_minutes = -420,
    };
    schedule.transitions[1] = (pomodoro_utc_offset_transition_t) {
        .effective_utc_epoch_seconds = 1000U,
        .offset_minutes = -360,
    };
    schedule.transitions[2] = (pomodoro_utc_offset_transition_t) {
        .effective_utc_epoch_seconds = 2000U,
        .offset_minutes = -420,
    };

    assert(pomodoro_time_zone_schedule_is_valid(&schedule));
    assert(pomodoro_time_zone_offset_minutes_at(&schedule, 999U, 0) == -420);
    assert(pomodoro_time_zone_offset_minutes_at(&schedule, 1000U, 0) == -360);
    assert(pomodoro_time_zone_offset_minutes_at(&schedule, 2500U, 0) == -420);
    assert(!pomodoro_time_zone_schedule_needs_sync(&schedule, 1000U));
    assert(pomodoro_time_zone_schedule_needs_sync(&schedule, 4000000U));

    schedule.transitions[2].effective_utc_epoch_seconds = 900U;
    assert(!pomodoro_time_zone_schedule_is_valid(&schedule));
    schedule.transitions[2].effective_utc_epoch_seconds = 3000U;
    schedule.transitions[2].offset_minutes = 17;
    assert(!pomodoro_time_zone_schedule_is_valid(&schedule));
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

static void test_legacy_v0_state_migrates_to_current(void)
{
    pomodoro_model_t restored;
    pomodoro_statistics_store_t restored_statistics;
    legacy_v1_persisted_state_t legacy;
    memset(&legacy, 0, sizeof(legacy));
    legacy.settings.value = (legacy_v1_settings_t) {
        .focus_minutes = 25U,
        .short_break_minutes = 5U,
        .long_break_minutes = 15U,
        .daily_goal = 8U,
        .auto_start_breaks = true,
        .volume = 3U,
        .brightness_percent = 70U,
    };
    legacy.timer = fresh_model().timer;
    legacy.timer.version = 0U;
    pomodoro_statistics_init(&legacy.statistics);
    legacy.statistics.version = 0U;
    legacy.checksum = pomodoro_test_crc32(
        &legacy, offsetof(legacy_v1_persisted_state_t, checksum));

    pomodoro_persisted_state_t current;
    memset(&current, 0, sizeof(current));
    memcpy(&current, &legacy, sizeof(legacy));

    bool was_migrated = false;
    assert(pomodoro_persistence_unpack(&current, &restored,
                                       &restored_statistics, &was_migrated));
    assert(was_migrated);
    assert(current.version == POMODORO_PERSISTED_STATE_VERSION);
    assert(current.settings.version == POMODORO_SETTINGS_VERSION);
    assert(current.timer.version == POMODORO_SNAPSHOT_VERSION);
    assert(current.statistics.version == POMODORO_STATISTICS_VERSION);
}

static void test_real_legacy_v1_layout_migrates_without_data_loss(void)
{
    pomodoro_model_t restored;
    pomodoro_statistics_store_t restored_statistics;
    legacy_v1_persisted_state_t legacy;
    memset(&legacy, 0, sizeof(legacy));
    legacy.version = 1U;
    legacy.settings.version = 1U;
    legacy.settings.value = (legacy_v1_settings_t) {
        .focus_minutes = 31U,
        .short_break_minutes = 7U,
        .long_break_minutes = 19U,
        .daily_goal = 11U,
        .auto_start_breaks = true,
        .auto_start_focus = false,
        .volume = 0U,
        .brightness_percent = 65U,
    };
    legacy.timer = fresh_model().timer;
    legacy.timer.timer_state = POMODORO_TIMER_PAUSED;
    legacy.timer.remaining_seconds = 321U;
    legacy.timer.set_progress = 2U;
    pomodoro_statistics_init(&legacy.statistics);
    legacy.statistics.pending_completed_pomodoros = 3U;
    legacy.checksum = pomodoro_test_crc32(
        &legacy, offsetof(legacy_v1_persisted_state_t, checksum));

    pomodoro_persisted_state_t current;
    memset(&current, 0, sizeof(current));
    memcpy(&current, &legacy, sizeof(legacy));
    bool migrated = false;
    assert(pomodoro_persistence_unpack(&current, &restored,
                                       &restored_statistics, &migrated));
    assert(migrated);
    assert(restored.settings.focus_minutes == 31U);
    assert(restored.settings.brightness_percent == 65U);
    assert(restored.timer.timer_state == POMODORO_TIMER_PAUSED);
    assert(restored.timer.remaining_seconds == 321U);
    assert(restored.timer.set_progress == 2U);
    assert(restored_statistics.pending_completed_pomodoros == 3U);
    assert(restored.last_nonzero_volume == 3U);
}

static void test_real_legacy_v2_journal_slot_migrates_without_data_loss(void)
{
    pomodoro_test_memory_journal_t memory;
    pomodoro_journal_t journal;
    memset(&memory, 0, sizeof(memory));
    pomodoro_test_memory_journal_init(&memory, &journal);

    legacy_v2_journal_slot_t legacy;
    memset(&legacy, 0, sizeof(legacy));
    legacy.generation = 7U;
    legacy.state.version = 2U;
    legacy.state.settings.version = 2U;
    legacy.state.settings.value = pomodoro_default_settings();
    legacy.state.settings.value.daily_goal = 13U;
    legacy.state.settings.last_nonzero_volume = 4U;
    legacy.state.timer = fresh_model().timer;
    legacy.state.timer.timer_state = POMODORO_TIMER_PAUSED;
    legacy.state.timer.remaining_seconds = 222U;
    pomodoro_statistics_init(&legacy.state.statistics);
    legacy.state.statistics.pending_completed_pomodoros = 2U;
    legacy.state.checksum = pomodoro_test_crc32(
        &legacy.state, offsetof(legacy_v2_persisted_state_t, checksum));
    legacy.checksum = pomodoro_test_crc32(
        &legacy, offsetof(legacy_v2_journal_slot_t, checksum));
    memcpy(&memory.slots[0], &legacy, sizeof(legacy));
    memory.present[0] = true;

    pomodoro_persisted_state_t loaded;
    assert(pomodoro_journal_load(&journal, &loaded) ==
           POMODORO_JOURNAL_READ_LOADED);
    pomodoro_model_t restored;
    pomodoro_statistics_store_t statistics;
    assert(pomodoro_persistence_unpack(&loaded, &restored, &statistics,
                                       NULL));
    assert(restored.settings.daily_goal == 13U);
    assert(restored.last_nonzero_volume == 4U);
    assert(restored.timer.timer_state == POMODORO_TIMER_PAUSED);
    assert(restored.timer.remaining_seconds == 222U);
    assert(statistics.pending_completed_pomodoros == 2U);
}

int main(void)
{
    test_focus_completes_once();
    test_pause_resume_excludes_paused_time();
    test_clock_rebase_preserves_running_remaining_time();
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
    test_statistics_evicts_oldest_of_366_days();
    test_local_time_conversion_uses_fixed_offset();
    test_time_zone_schedule_applies_transitions_and_expiry();
    test_persistence_detects_corruption();
    test_journal_survives_torn_write();
    test_legacy_v0_state_migrates_to_current();
    test_real_legacy_v1_layout_migrates_without_data_loss();
    test_real_legacy_v2_journal_slot_migrates_without_data_loss();
    puts("pomodoro_core_test: all tests passed");
    return 0;
}
