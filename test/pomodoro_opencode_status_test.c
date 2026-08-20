#include "pomodoro_opencode_status.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static void test_parse_rejects_malformed_lines(void)
{
    uint32_t revision = 0U;
    uint8_t mask = 0U;

    assert(pomodoro_opencode_parse_line("OC1 STATE 3 5", 13, &revision,
                                        &mask) ==
           POMODORO_OPENCODE_LINE_STATE);
    assert(revision == 3U);
    assert(mask == 5U);
    assert(pomodoro_opencode_parse_line("OC1 STATE 4 15", 14, &revision,
                                        &mask) ==
           POMODORO_OPENCODE_LINE_STATE);
    assert(revision == 4U);
    assert(mask == 15U);

    assert(pomodoro_opencode_parse_line("", 0U, &revision, &mask) ==
           POMODORO_OPENCODE_LINE_NONE);
    assert(pomodoro_opencode_parse_line("log output from console", 22,
                                        &revision, &mask) ==
           POMODORO_OPENCODE_LINE_NONE);
    assert(pomodoro_opencode_parse_line("OC2 STATE 1 2", 14, &revision,
                                        &mask) ==
           POMODORO_OPENCODE_LINE_NONE);
    assert(pomodoro_opencode_parse_line("OC1 NOPE 1 2", 13, &revision,
                                        &mask) ==
           POMODORO_OPENCODE_LINE_INVALID);
    assert(pomodoro_opencode_parse_line("OC1 STATE 1", 12, &revision,
                                        &mask) ==
           POMODORO_OPENCODE_LINE_INVALID);
    assert(pomodoro_opencode_parse_line("OC1 STATE 1 2 3", 16, &revision,
                                        &mask) ==
           POMODORO_OPENCODE_LINE_INVALID);
    assert(pomodoro_opencode_parse_line("OC1 STATE x 2", 14, &revision,
                                        &mask) ==
           POMODORO_OPENCODE_LINE_INVALID);
    assert(pomodoro_opencode_parse_line("OC1 STATE 1 y", 14, &revision,
                                        &mask) ==
           POMODORO_OPENCODE_LINE_INVALID);
    assert(pomodoro_opencode_parse_line("OC1 STATE 1 16", 15, &revision,
                                        &mask) ==
           POMODORO_OPENCODE_LINE_INVALID);
    assert(pomodoro_opencode_parse_line("OC1 STATE 4294967296 1", 22,
                                        &revision, &mask) ==
           POMODORO_OPENCODE_LINE_INVALID);
    assert(pomodoro_opencode_parse_line("OC1 STATE 0 0", 13, &revision,
                                        &mask) ==
           POMODORO_OPENCODE_LINE_STATE);
    assert(revision == 0U);
    assert(mask == 0U);

    char long_line[POMODORO_OPENCODE_MAX_LINE_LENGTH + 8];
    memset(long_line, 'a', sizeof(long_line));
    memcpy(long_line, "OC1 STATE 1 2", 14);
    assert(pomodoro_opencode_parse_line(
               long_line, sizeof(long_line), &revision, &mask) ==
           POMODORO_OPENCODE_LINE_NONE);
}

static void test_priority_and_revision_dedup(void)
{
    pomodoro_opencode_status_t status;
    pomodoro_opencode_status_init(&status, true, 0U);

    assert(pomodoro_opencode_status_resolve(&status, 0U) ==
           POMODORO_OPENCODE_STATE_HIDDEN);

    pomodoro_opencode_status_apply_state(&status, 1U, 0x0FU, 10U);
    assert(pomodoro_opencode_status_connected(&status));
    assert(pomodoro_opencode_status_resolve(&status, 10U) ==
           POMODORO_OPENCODE_STATE_ERROR);
    assert(pomodoro_opencode_status_take_wake(&status));

    /* Repeated heartbeat with the same revision and mask: no wake. */
    pomodoro_opencode_status_apply_state(&status, 1U, 0x0FU, 13U);
    assert(!pomodoro_opencode_status_take_wake(&status));
    assert(pomodoro_opencode_status_resolve(&status, 13U) ==
           POMODORO_OPENCODE_STATE_ERROR);

    /* Waiting outranks working, working outranks completed. */
    pomodoro_opencode_status_apply_state(&status, 2U, 0x0EU, 20U);
    assert(pomodoro_opencode_status_resolve(&status, 20U) ==
           POMODORO_OPENCODE_STATE_WAITING);
    assert(pomodoro_opencode_status_take_wake(&status));

    pomodoro_opencode_status_apply_state(&status, 3U, 0x0CU, 30U);
    assert(pomodoro_opencode_status_resolve(&status, 30U) ==
           POMODORO_OPENCODE_STATE_WORKING);
    assert(pomodoro_opencode_status_take_wake(&status));

    pomodoro_opencode_status_apply_state(&status, 4U, 0x08U, 40U);
    assert(pomodoro_opencode_status_resolve(&status, 40U) ==
           POMODORO_OPENCODE_STATE_COMPLETED);
    assert(pomodoro_opencode_status_take_wake(&status));

    pomodoro_opencode_status_apply_state(&status, 5U, 0x00U, 50U);
    assert(pomodoro_opencode_status_resolve(&status, 50U) ==
           POMODORO_OPENCODE_STATE_IDLE);
    assert(pomodoro_opencode_status_take_wake(&status));

    /* An older revision cannot replace the current aggregate state. */
    pomodoro_opencode_status_apply_state(&status, 4U, 0x01U, 51U);
    assert(pomodoro_opencode_status_resolve(&status, 51U) ==
           POMODORO_OPENCODE_STATE_IDLE);
    assert(!pomodoro_opencode_status_take_wake(&status));

    /* Same-revision mask change still updates the display without a wake. */
    pomodoro_opencode_status_apply_state(&status, 5U, 0x02U, 60U);
    assert(!pomodoro_opencode_status_take_wake(&status));
    assert(pomodoro_opencode_status_resolve(&status, 60U) ==
           POMODORO_OPENCODE_STATE_WAITING);
}

static void test_completed_expires_after_five_seconds(void)
{
    pomodoro_opencode_status_t status;
    pomodoro_opencode_status_init(&status, true, 0U);

pomodoro_opencode_status_apply_state(&status, 1U, 0x08U, 100U);
    assert(pomodoro_opencode_status_resolve(&status, 100U) ==
           POMODORO_OPENCODE_STATE_COMPLETED);
    assert(pomodoro_opencode_status_take_wake(&status));
    pomodoro_opencode_status_tick(&status, 104U);
    assert(pomodoro_opencode_status_resolve(&status, 104U) ==
           POMODORO_OPENCODE_STATE_COMPLETED);

    /* At exactly the five-second mark the Completed presentation expires. */
    assert(pomodoro_opencode_status_resolve(&status, 105U) ==
           POMODORO_OPENCODE_STATE_IDLE);

    /* A heartbeat with the same revision must not restart the five seconds. */
    pomodoro_opencode_status_apply_state(&status, 1U, 0x08U, 106U);
    assert(pomodoro_opencode_status_resolve(&status, 106U) ==
           POMODORO_OPENCODE_STATE_IDLE);
    assert(!pomodoro_opencode_status_take_wake(&status));

    /* A new revision with Completed starts a fresh presentation. */
    pomodoro_opencode_status_apply_state(&status, 2U, 0x08U, 110U);
    assert(pomodoro_opencode_status_resolve(&status, 110U) ==
           POMODORO_OPENCODE_STATE_COMPLETED);
}

static void test_ack_clears_only_the_current_revision(void)
{
    pomodoro_opencode_status_t status;
    uint8_t acknowledged = 0U;
    pomodoro_opencode_status_init(&status, true, 0U);

    pomodoro_opencode_status_apply_state(&status, 1U, 0x0FU, 10U);
    assert(pomodoro_opencode_status_resolve(&status, 10U) ==
           POMODORO_OPENCODE_STATE_ERROR);
    assert(pomodoro_opencode_status_take_wake(&status));
    assert(pomodoro_opencode_status_ack(&status, 10U, &acknowledged));
    assert(acknowledged == POMODORO_OPENCODE_BIT_ERROR);
    assert(pomodoro_opencode_status_resolve(&status, 10U) ==
           POMODORO_OPENCODE_STATE_WAITING);

    /* Nothing left to acknowledge once Error is cleared. */
    assert(!pomodoro_opencode_status_ack(&status, 10U, &acknowledged));

    /* Same-revision heartbeat keeps the ack in place. */
    pomodoro_opencode_status_apply_state(&status, 1U, 0x0FU, 13U);
    assert(pomodoro_opencode_status_resolve(&status, 13U) ==
           POMODORO_OPENCODE_STATE_WAITING);
    assert(!pomodoro_opencode_status_take_wake(&status));

    /* A new error is a new revision and is not cleared by the old ack. */
    pomodoro_opencode_status_apply_state(&status, 2U, 0x0FU, 20U);
    assert(pomodoro_opencode_status_resolve(&status, 20U) ==
           POMODORO_OPENCODE_STATE_ERROR);
    assert(pomodoro_opencode_status_take_wake(&status));
    assert(pomodoro_opencode_status_ack(&status, 20U, &acknowledged));
    assert(acknowledged == POMODORO_OPENCODE_BIT_ERROR);
    assert(pomodoro_opencode_status_resolve(&status, 20U) ==
           POMODORO_OPENCODE_STATE_WAITING);

    /* Completed can be acknowledged before expiry. */
    pomodoro_opencode_status_apply_state(&status, 3U, 0x08U, 30U);
    assert(pomodoro_opencode_status_resolve(&status, 30U) ==
           POMODORO_OPENCODE_STATE_COMPLETED);
    assert(pomodoro_opencode_status_ack(&status, 30U, &acknowledged));
    assert(acknowledged == POMODORO_OPENCODE_BIT_COMPLETED);
    assert(pomodoro_opencode_status_resolve(&status, 30U) ==
           POMODORO_OPENCODE_STATE_IDLE);

    /* After ack, an expired Completed is no longer acknowledgeable. */
    pomodoro_opencode_status_apply_state(&status, 4U, 0x08U, 40U);
    assert(pomodoro_opencode_status_resolve(&status, 45U) ==
           POMODORO_OPENCODE_STATE_IDLE);
    assert(!pomodoro_opencode_status_ack(&status, 45U, &acknowledged));
}

static void test_disconnect_hides_after_ten_seconds(void)
{
    pomodoro_opencode_status_t status;
    pomodoro_opencode_status_init(&status, true, 0U);

    pomodoro_opencode_status_apply_state(&status, 1U, 0x02U, 100U);
    assert(pomodoro_opencode_status_resolve(&status, 100U) ==
           POMODORO_OPENCODE_STATE_WAITING);
    pomodoro_opencode_status_tick(&status, 109U);
    assert(pomodoro_opencode_status_resolve(&status, 109U) ==
           POMODORO_OPENCODE_STATE_WAITING);
    assert(pomodoro_opencode_status_take_wake(&status));

    pomodoro_opencode_status_tick(&status, 110U);
    assert(pomodoro_opencode_status_resolve(&status, 110U) ==
           POMODORO_OPENCODE_STATE_HIDDEN);
    assert(!pomodoro_opencode_status_connected(&status));

    /* Reconnect with the same revision restores the display without a wake. */
    pomodoro_opencode_status_apply_state(&status, 1U, 0x02U, 111U);
    assert(pomodoro_opencode_status_resolve(&status, 111U) ==
           POMODORO_OPENCODE_STATE_WAITING);
    assert(!pomodoro_opencode_status_take_wake(&status));

    /* Reconnect with a changed state is a real change and wakes. */
    pomodoro_opencode_status_apply_state(&status, 2U, 0x01U, 112U);
    assert(pomodoro_opencode_status_resolve(&status, 112U) ==
           POMODORO_OPENCODE_STATE_ERROR);
    assert(pomodoro_opencode_status_take_wake(&status));
}

static void test_disabling_hides_and_forbids_wakes_and_acks(void)
{
    pomodoro_opencode_status_t status;
    uint8_t acknowledged = 0U;
    pomodoro_opencode_status_init(&status, true, 0U);

    pomodoro_opencode_status_apply_state(&status, 1U, 0x01U, 10U);
    assert(pomodoro_opencode_status_resolve(&status, 10U) ==
           POMODORO_OPENCODE_STATE_ERROR);

    pomodoro_opencode_status_set_enabled(&status, false, 11U);
    assert(pomodoro_opencode_status_resolve(&status, 11U) ==
           POMODORO_OPENCODE_STATE_HIDDEN);
    assert(!pomodoro_opencode_status_connected(&status));
    assert(!pomodoro_opencode_status_ack(&status, 11U, &acknowledged));

    /* Disabled engine ignores state reads entirely. */
    pomodoro_opencode_status_apply_state(&status, 2U, 0x01U, 12U);
    assert(pomodoro_opencode_status_resolve(&status, 12U) ==
           POMODORO_OPENCODE_STATE_HIDDEN);
    assert(!pomodoro_opencode_status_take_wake(&status));

    /* Re-enabling restores the previous snapshot on reconnect. */
    pomodoro_opencode_status_set_enabled(&status, true, 20U);
    pomodoro_opencode_status_apply_state(&status, 1U, 0x01U, 21U);
    assert(pomodoro_opencode_status_resolve(&status, 21U) ==
           POMODORO_OPENCODE_STATE_ERROR);
    assert(!pomodoro_opencode_status_take_wake(&status));

    /* A changed state after re-enable is a real change and wakes. */
    pomodoro_opencode_status_apply_state(&status, 3U, 0x04U, 22U);
    assert(pomodoro_opencode_status_resolve(&status, 22U) ==
           POMODORO_OPENCODE_STATE_WORKING);
    assert(pomodoro_opencode_status_take_wake(&status));
}

int main(void)
{
    test_parse_rejects_malformed_lines();
    test_priority_and_revision_dedup();
    test_completed_expires_after_five_seconds();
    test_ack_clears_only_the_current_revision();
    test_disconnect_hides_after_ten_seconds();
    test_disabling_hides_and_forbids_wakes_and_acks();
    puts("pomodoro_opencode_status_test: all tests passed");
    return 0;
}
