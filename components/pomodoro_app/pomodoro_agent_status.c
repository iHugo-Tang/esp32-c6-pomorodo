#include "pomodoro_agent_status.h"

#include <stddef.h>

static uint8_t mask_bits(uint8_t mask)
{
    return mask & (POMODORO_AGENT_STATUS_ERROR_BIT |
                   POMODORO_AGENT_STATUS_WAITING_BIT |
                   POMODORO_AGENT_STATUS_WORKING_BIT |
                   POMODORO_AGENT_STATUS_COMPLETED_BIT);
}

void pomodoro_agent_status_init(pomodoro_agent_status_t *status, bool enabled,
                                uint64_t now_wall_time_seconds)
{
    if (status == NULL) {
        return;
    }
    status->enabled = enabled;
    status->connected = false;
    status->has_revision = false;
    status->revision = 0U;
    status->raw_mask = 0U;
    status->locally_cleared = 0U;
    status->completed_deadline_wall_time_seconds = 0U;
    status->last_heartbeat_wall_time_seconds = now_wall_time_seconds;
    status->wake_pending = false;
}

void pomodoro_agent_status_apply(pomodoro_agent_status_t *status,
                                 uint32_t revision, uint8_t mask,
                                 uint64_t now_wall_time_seconds)
{
    if (status == NULL || !status->enabled) {
        return;
    }
    uint8_t next_mask = mask_bits(mask);
    if (status->connected && status->has_revision &&
        (revision < status->revision ||
         (revision == status->revision && next_mask != status->raw_mask))) {
        return;
    }
    bool first_state = !status->has_revision;
    bool reconnecting = status->has_revision && !status->connected;
    bool revision_changed = status->has_revision &&
        revision != status->revision;
    bool mask_changed = status->has_revision && next_mask != status->raw_mask;
    bool new_state_epoch = first_state || revision_changed ||
        (reconnecting && mask_changed);
    status->has_revision = true;
    status->connected = true;
    status->last_heartbeat_wall_time_seconds = now_wall_time_seconds;
    status->revision = revision;
    status->raw_mask = next_mask;
    if (new_state_epoch) {
        status->locally_cleared = 0U;
        if ((status->raw_mask & POMODORO_AGENT_STATUS_COMPLETED_BIT) != 0U) {
            status->completed_deadline_wall_time_seconds =
                now_wall_time_seconds +
                POMODORO_AGENT_STATUS_COMPLETED_DISPLAY_SECONDS;
        } else {
            status->completed_deadline_wall_time_seconds = 0U;
        }
        status->wake_pending = true;
    } else if ((status->raw_mask & POMODORO_AGENT_STATUS_COMPLETED_BIT) == 0U) {
        status->completed_deadline_wall_time_seconds = 0U;
    }
}

void pomodoro_agent_status_tick(pomodoro_agent_status_t *status,
                                uint64_t now_wall_time_seconds)
{
    if (status == NULL) {
        return;
    }
    if (!status->enabled) {
        status->connected = false;
        status->completed_deadline_wall_time_seconds = 0U;
        return;
    }
    if (status->connected &&
        now_wall_time_seconds >=
            status->last_heartbeat_wall_time_seconds +
                POMODORO_AGENT_STATUS_HEARTBEAT_TIMEOUT_SECONDS) {
        status->connected = false;
        status->completed_deadline_wall_time_seconds = 0U;
    }
}

void pomodoro_agent_status_set_enabled(pomodoro_agent_status_t *status,
                                       bool enabled,
                                       uint64_t now_wall_time_seconds)
{
    if (status == NULL) {
        return;
    }
    status->enabled = enabled;
    status->wake_pending = false;
    if (!enabled) {
        status->connected = false;
        status->completed_deadline_wall_time_seconds = 0U;
        status->locally_cleared = 0U;
    }
    (void) now_wall_time_seconds;
}

bool pomodoro_agent_status_acknowledge(pomodoro_agent_status_t *status,
                                       uint64_t now_wall_time_seconds,
                                       uint8_t *acknowledged_mask)
{
    if (status == NULL || acknowledged_mask == NULL) {
        return false;
    }
    *acknowledged_mask = 0U;
    if (!status->enabled || !status->connected) {
        return false;
    }
    pomodoro_agent_status_state_t state =
        pomodoro_agent_status_resolve(status, now_wall_time_seconds);
    uint8_t bit = 0U;
    if (state == POMODORO_AGENT_STATUS_ERROR) {
        bit = POMODORO_AGENT_STATUS_ERROR_BIT;
    } else if (state == POMODORO_AGENT_STATUS_COMPLETED) {
        bit = POMODORO_AGENT_STATUS_COMPLETED_BIT;
    } else {
        return false;
    }
    status->locally_cleared = (uint8_t) (status->locally_cleared | bit);
    if (bit == POMODORO_AGENT_STATUS_COMPLETED_BIT) {
        status->completed_deadline_wall_time_seconds = 0U;
    }
    *acknowledged_mask = bit;
    return true;
}

bool pomodoro_agent_status_take_wake(pomodoro_agent_status_t *status)
{
    if (status == NULL) {
        return false;
    }
    bool wake = status->wake_pending;
    status->wake_pending = false;
    return wake;
}

pomodoro_agent_status_state_t pomodoro_agent_status_resolve(
    const pomodoro_agent_status_t *status, uint64_t now_wall_time_seconds)
{
    if (status == NULL || !status->enabled || !status->connected ||
        !status->has_revision) {
        return POMODORO_AGENT_STATUS_IDLE;
    }
    uint8_t active = (uint8_t) (status->raw_mask & ~status->locally_cleared);
    if ((active & POMODORO_AGENT_STATUS_ERROR_BIT) != 0U) {
        return POMODORO_AGENT_STATUS_ERROR;
    }
    if ((active & POMODORO_AGENT_STATUS_WAITING_BIT) != 0U) {
        return POMODORO_AGENT_STATUS_WAITING;
    }
    if ((active & POMODORO_AGENT_STATUS_WORKING_BIT) != 0U) {
        return POMODORO_AGENT_STATUS_WORKING;
    }
    if ((active & POMODORO_AGENT_STATUS_COMPLETED_BIT) != 0U &&
        status->completed_deadline_wall_time_seconds != 0U &&
        now_wall_time_seconds < status->completed_deadline_wall_time_seconds) {
        return POMODORO_AGENT_STATUS_COMPLETED;
    }
    return POMODORO_AGENT_STATUS_IDLE;
}

bool pomodoro_agent_status_connected(const pomodoro_agent_status_t *status)
{
    return status != NULL && status->enabled && status->connected;
}
