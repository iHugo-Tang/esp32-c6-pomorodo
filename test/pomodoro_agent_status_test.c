#include "pomodoro_agent_status.h"

#include <assert.h>
#include <stdio.h>

static void test_priority_acknowledgement_and_wake(void)
{
    pomodoro_agent_status_t status;
    uint8_t acknowledged = 0U;
    pomodoro_agent_status_init(&status, true, 0U);

    assert(!pomodoro_agent_status_connected(&status));
    pomodoro_agent_status_apply(&status, 1U, 0x0FU, 10U);
    assert(pomodoro_agent_status_connected(&status));
    assert(pomodoro_agent_status_resolve(&status, 10U) ==
           POMODORO_AGENT_STATUS_ERROR);
    assert(pomodoro_agent_status_take_wake(&status));
    assert(pomodoro_agent_status_acknowledge(&status, 10U, &acknowledged));
    assert(acknowledged == POMODORO_AGENT_STATUS_ERROR_BIT);
    assert(pomodoro_agent_status_resolve(&status, 10U) ==
           POMODORO_AGENT_STATUS_WAITING);

    pomodoro_agent_status_apply(&status, 1U, 0x0FU, 13U);
    assert(!pomodoro_agent_status_take_wake(&status));
    assert(pomodoro_agent_status_resolve(&status, 13U) ==
           POMODORO_AGENT_STATUS_WAITING);
}

static void test_completed_timeout_stale_revision_and_disconnect(void)
{
    pomodoro_agent_status_t status;
    pomodoro_agent_status_init(&status, true, 0U);

    pomodoro_agent_status_apply(&status, 2U,
                                POMODORO_AGENT_STATUS_COMPLETED_BIT, 100U);
    assert(pomodoro_agent_status_resolve(&status, 104U) ==
           POMODORO_AGENT_STATUS_COMPLETED);
    assert(pomodoro_agent_status_resolve(&status, 105U) ==
           POMODORO_AGENT_STATUS_IDLE);
    assert(pomodoro_agent_status_take_wake(&status));

    pomodoro_agent_status_apply(&status, 1U,
                                POMODORO_AGENT_STATUS_ERROR_BIT, 106U);
    assert(pomodoro_agent_status_resolve(&status, 106U) ==
           POMODORO_AGENT_STATUS_IDLE);
    assert(!pomodoro_agent_status_take_wake(&status));

    pomodoro_agent_status_tick(&status, 109U);
    assert(pomodoro_agent_status_connected(&status));
    pomodoro_agent_status_tick(&status, 110U);
    assert(!pomodoro_agent_status_connected(&status));
    assert(pomodoro_agent_status_resolve(&status, 110U) ==
           POMODORO_AGENT_STATUS_IDLE);
}

static void test_disable_requires_resync(void)
{
    pomodoro_agent_status_t status;
    uint8_t acknowledged = 0U;
    pomodoro_agent_status_init(&status, true, 0U);
    pomodoro_agent_status_apply(&status, 1U,
                                POMODORO_AGENT_STATUS_ERROR_BIT, 1U);

    pomodoro_agent_status_set_enabled(&status, false, 2U);
    assert(!pomodoro_agent_status_connected(&status));
    assert(!pomodoro_agent_status_take_wake(&status));
    assert(!pomodoro_agent_status_acknowledge(&status, 2U, &acknowledged));

    pomodoro_agent_status_set_enabled(&status, true, 3U);
    assert(!pomodoro_agent_status_connected(&status));
    pomodoro_agent_status_apply(&status, 1U,
                                POMODORO_AGENT_STATUS_ERROR_BIT, 4U);
    assert(pomodoro_agent_status_resolve(&status, 4U) ==
           POMODORO_AGENT_STATUS_ERROR);
}

int main(void)
{
    test_priority_acknowledgement_and_wake();
    test_completed_timeout_stale_revision_and_disconnect();
    test_disable_requires_resync();
    puts("pomodoro_agent_status_test: all tests passed");
    return 0;
}
