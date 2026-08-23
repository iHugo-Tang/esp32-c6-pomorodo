#ifndef POMODORO_AGENT_STATUS_H
#define POMODORO_AGENT_STATUS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define POMODORO_AGENT_STATUS_HEARTBEAT_TIMEOUT_SECONDS 10U
#define POMODORO_AGENT_STATUS_COMPLETED_DISPLAY_SECONDS 5U

typedef enum {
    POMODORO_AGENT_STATUS_ERROR_BIT = 1U << 0,
    POMODORO_AGENT_STATUS_WAITING_BIT = 1U << 1,
    POMODORO_AGENT_STATUS_WORKING_BIT = 1U << 2,
    POMODORO_AGENT_STATUS_COMPLETED_BIT = 1U << 3,
} pomodoro_agent_status_bit_t;

typedef enum {
    POMODORO_AGENT_STATUS_IDLE = 0,
    POMODORO_AGENT_STATUS_WORKING,
    POMODORO_AGENT_STATUS_WAITING,
    POMODORO_AGENT_STATUS_COMPLETED,
    POMODORO_AGENT_STATUS_ERROR,
} pomodoro_agent_status_state_t;

typedef struct {
    bool enabled;
    bool connected;
    bool has_revision;
    uint32_t revision;
    uint8_t raw_mask;
    uint8_t locally_cleared;
    uint64_t completed_deadline_wall_time_seconds;
    uint64_t last_heartbeat_wall_time_seconds;
    bool wake_pending;
} pomodoro_agent_status_t;

void pomodoro_agent_status_init(pomodoro_agent_status_t *status, bool enabled,
                                uint64_t now_wall_time_seconds);
void pomodoro_agent_status_apply(pomodoro_agent_status_t *status,
                                 uint32_t revision, uint8_t mask,
                                 uint64_t now_wall_time_seconds);
void pomodoro_agent_status_tick(pomodoro_agent_status_t *status,
                                uint64_t now_wall_time_seconds);
void pomodoro_agent_status_set_enabled(pomodoro_agent_status_t *status,
                                       bool enabled,
                                       uint64_t now_wall_time_seconds);
bool pomodoro_agent_status_acknowledge(pomodoro_agent_status_t *status,
                                       uint64_t now_wall_time_seconds,
                                       uint8_t *acknowledged_mask);
bool pomodoro_agent_status_take_wake(pomodoro_agent_status_t *status);
pomodoro_agent_status_state_t pomodoro_agent_status_resolve(
    const pomodoro_agent_status_t *status, uint64_t now_wall_time_seconds);
bool pomodoro_agent_status_connected(const pomodoro_agent_status_t *status);

#ifdef __cplusplus
}
#endif

#endif
