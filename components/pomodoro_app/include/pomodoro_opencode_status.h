#ifndef POMODORO_OPENCODE_STATUS_H
#define POMODORO_OPENCODE_STATUS_H

#include "pomodoro_agent_status.h"
#include "pomodoro_oc1_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Temporary expand-contract aliases. New code uses Agent Status and OC1. */
#define POMODORO_OPENCODE_MAX_LINE_LENGTH POMODORO_OC1_MAX_LINE_LENGTH
#define POMODORO_OPENCODE_CONTROL_INTERVAL_SECONDS \
    POMODORO_OC1_CONTROL_INTERVAL_SECONDS
#define POMODORO_OPENCODE_HEARTBEAT_TIMEOUT_SECONDS \
    POMODORO_AGENT_STATUS_HEARTBEAT_TIMEOUT_SECONDS
#define POMODORO_OPENCODE_COMPLETED_DISPLAY_SECONDS \
    POMODORO_AGENT_STATUS_COMPLETED_DISPLAY_SECONDS

typedef pomodoro_agent_status_bit_t pomodoro_opencode_bit_t;
#define POMODORO_OPENCODE_BIT_ERROR POMODORO_AGENT_STATUS_ERROR_BIT
#define POMODORO_OPENCODE_BIT_WAITING POMODORO_AGENT_STATUS_WAITING_BIT
#define POMODORO_OPENCODE_BIT_WORKING POMODORO_AGENT_STATUS_WORKING_BIT
#define POMODORO_OPENCODE_BIT_COMPLETED POMODORO_AGENT_STATUS_COMPLETED_BIT

typedef enum {
    POMODORO_OPENCODE_STATE_IDLE = POMODORO_AGENT_STATUS_IDLE,
    POMODORO_OPENCODE_STATE_WORKING = POMODORO_AGENT_STATUS_WORKING,
    POMODORO_OPENCODE_STATE_WAITING = POMODORO_AGENT_STATUS_WAITING,
    POMODORO_OPENCODE_STATE_COMPLETED = POMODORO_AGENT_STATUS_COMPLETED,
    POMODORO_OPENCODE_STATE_ERROR = POMODORO_AGENT_STATUS_ERROR,
    POMODORO_OPENCODE_STATE_HIDDEN,
} pomodoro_opencode_state_t;

typedef pomodoro_oc1_line_kind_t pomodoro_opencode_line_kind_t;
#define POMODORO_OPENCODE_LINE_NONE POMODORO_OC1_LINE_NONE
#define POMODORO_OPENCODE_LINE_STATE POMODORO_OC1_LINE_STATE
#define POMODORO_OPENCODE_LINE_INVALID POMODORO_OC1_LINE_INVALID

typedef pomodoro_agent_status_t pomodoro_opencode_status_t;

pomodoro_opencode_line_kind_t pomodoro_opencode_parse_line(
    const char *line, size_t length, uint32_t *revision, uint8_t *mask);
void pomodoro_opencode_status_init(pomodoro_opencode_status_t *status,
                                   bool enabled,
                                   uint64_t now_wall_time_seconds);
void pomodoro_opencode_status_apply_state(
    pomodoro_opencode_status_t *status, uint32_t revision, uint8_t mask,
    uint64_t now_wall_time_seconds);
void pomodoro_opencode_status_tick(pomodoro_opencode_status_t *status,
                                   uint64_t now_wall_time_seconds);
void pomodoro_opencode_status_set_enabled(pomodoro_opencode_status_t *status,
                                          bool enabled,
                                          uint64_t now_wall_time_seconds);
bool pomodoro_opencode_status_ack(pomodoro_opencode_status_t *status,
                                  uint64_t now_wall_time_seconds,
                                  uint8_t *acknowledged_mask);
bool pomodoro_opencode_status_take_wake(pomodoro_opencode_status_t *status);
pomodoro_opencode_state_t pomodoro_opencode_status_resolve(
    const pomodoro_opencode_status_t *status,
    uint64_t now_wall_time_seconds);
bool pomodoro_opencode_status_connected(
    const pomodoro_opencode_status_t *status);

#ifdef __cplusplus
}
#endif

#endif
