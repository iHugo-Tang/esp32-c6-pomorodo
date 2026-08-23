#include "pomodoro_oc1_protocol.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_parse_state(void)
{
    uint32_t revision = 0U;
    uint8_t mask = 0U;

    assert(pomodoro_oc1_parse_line("OC1 STATE 3 15", 14U, &revision, &mask) ==
           POMODORO_OC1_LINE_STATE);
    assert(revision == 3U);
    assert(mask == 15U);

    assert(pomodoro_oc1_parse_line("console output", 14U, &revision, &mask) ==
           POMODORO_OC1_LINE_NONE);
    assert(pomodoro_oc1_parse_line("OC2 STATE 1 2", 13U, &revision, &mask) ==
           POMODORO_OC1_LINE_NONE);
    assert(pomodoro_oc1_parse_line("OC1 NOPE 1 2", 12U, &revision, &mask) ==
           POMODORO_OC1_LINE_INVALID);
    assert(pomodoro_oc1_parse_line("OC1 STATE 1", 11U, &revision, &mask) ==
           POMODORO_OC1_LINE_INVALID);
    assert(pomodoro_oc1_parse_line("OC1 STATE 1 2 3", 15U, &revision, &mask) ==
           POMODORO_OC1_LINE_INVALID);
    assert(pomodoro_oc1_parse_line("OC1 STATE x 2", 13U, &revision, &mask) ==
           POMODORO_OC1_LINE_INVALID);
    assert(pomodoro_oc1_parse_line("OC1 STATE 1 16", 14U, &revision, &mask) ==
           POMODORO_OC1_LINE_INVALID);
    assert(pomodoro_oc1_parse_line("OC1 STATE 4294967296 1", 22U, &revision,
                                   &mask) == POMODORO_OC1_LINE_INVALID);

    char long_line[POMODORO_OC1_MAX_LINE_LENGTH + 1U];
    memset(long_line, 'x', sizeof(long_line));
    memcpy(long_line, "OC1 STATE 1 2", 13U);
    assert(pomodoro_oc1_parse_line(long_line, sizeof(long_line), &revision,
                                   &mask) == POMODORO_OC1_LINE_NONE);

    char maximum_line[POMODORO_OC1_MAX_LINE_LENGTH];
    memset(maximum_line, '0', sizeof(maximum_line));
    memcpy(maximum_line, "OC1 STATE 1 ", 12U);
    assert(pomodoro_oc1_parse_line(maximum_line, sizeof(maximum_line),
                                   &revision, &mask) ==
           POMODORO_OC1_LINE_STATE);
    assert(revision == 1U);
    assert(mask == 0U);
}

static void test_format_control_and_ack(void)
{
    char line[POMODORO_OC1_MAX_LINE_LENGTH + 1U];

    assert(pomodoro_oc1_format_control(true, line, sizeof(line)));
    assert(strcmp(line, "OC1 ENABLED") == 0);
    assert(pomodoro_oc1_format_control(false, line, sizeof(line)));
    assert(strcmp(line, "OC1 DISABLED") == 0);
    assert(pomodoro_oc1_format_ack(42U, 1U, line, sizeof(line)));
    assert(strcmp(line, "OC1 ACK 42 1") == 0);
    assert(pomodoro_oc1_format_ack(UINT32_MAX, 8U, line, sizeof(line)));
    assert(strcmp(line, "OC1 ACK 4294967295 8") == 0);
    assert(!pomodoro_oc1_format_ack(1U, 2U, line, sizeof(line)));
    assert(!pomodoro_oc1_format_control(true, line, 4U));
}

int main(void)
{
    test_parse_state();
    test_format_control_and_ack();
    puts("pomodoro_oc1_protocol_test: all tests passed");
    return 0;
}
