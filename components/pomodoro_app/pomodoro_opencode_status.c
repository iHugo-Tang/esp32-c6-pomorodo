#include "pomodoro_opencode_status.h"

pomodoro_opencode_line_kind_t pomodoro_opencode_parse_line(
    const char *line, size_t length, uint32_t *revision, uint8_t *mask)
{
    return pomodoro_oc1_parse_line(line, length, revision, mask);
}

void pomodoro_opencode_status_init(pomodoro_opencode_status_t *status,
                                   bool enabled,
                                   uint64_t now_wall_time_seconds)
{
    pomodoro_agent_status_init(status, enabled, now_wall_time_seconds);
}

void pomodoro_opencode_status_apply_state(
    pomodoro_opencode_status_t *status, uint32_t revision, uint8_t mask,
    uint64_t now_wall_time_seconds)
{
    pomodoro_agent_status_apply(status, revision, mask,
                                now_wall_time_seconds);
}

void pomodoro_opencode_status_tick(pomodoro_opencode_status_t *status,
                                   uint64_t now_wall_time_seconds)
{
    pomodoro_agent_status_tick(status, now_wall_time_seconds);
}

void pomodoro_opencode_status_set_enabled(pomodoro_opencode_status_t *status,
                                          bool enabled,
                                          uint64_t now_wall_time_seconds)
{
    pomodoro_agent_status_set_enabled(status, enabled,
                                      now_wall_time_seconds);
}

bool pomodoro_opencode_status_ack(pomodoro_opencode_status_t *status,
                                  uint64_t now_wall_time_seconds,
                                  uint8_t *acknowledged_mask)
{
    return pomodoro_agent_status_acknowledge(status, now_wall_time_seconds,
                                             acknowledged_mask);
}

bool pomodoro_opencode_status_take_wake(pomodoro_opencode_status_t *status)
{
    return pomodoro_agent_status_take_wake(status);
}

pomodoro_opencode_state_t pomodoro_opencode_status_resolve(
    const pomodoro_opencode_status_t *status,
    uint64_t now_wall_time_seconds)
{
    if (status == NULL || !status->enabled || !status->connected ||
        !status->has_revision) {
        return POMODORO_OPENCODE_STATE_HIDDEN;
    }
    return (pomodoro_opencode_state_t)
        pomodoro_agent_status_resolve(status, now_wall_time_seconds);
}

bool pomodoro_opencode_status_connected(
    const pomodoro_opencode_status_t *status)
{
    return pomodoro_agent_status_connected(status);
}
