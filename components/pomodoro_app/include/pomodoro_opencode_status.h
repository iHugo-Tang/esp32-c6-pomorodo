#ifndef POMODORO_OPENCODE_STATUS_H
#define POMODORO_OPENCODE_STATUS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define POMODORO_OPENCODE_MAX_LINE_LENGTH 64U
#define POMODORO_OPENCODE_HEARTBEAT_TIMEOUT_SECONDS 10U
#define POMODORO_OPENCODE_COMPLETED_DISPLAY_SECONDS 5U
#define POMODORO_OPENCODE_CONTROL_INTERVAL_SECONDS 3U

typedef enum {
    POMODORO_OPENCODE_BIT_ERROR = 1U << 0,
    POMODORO_OPENCODE_BIT_WAITING = 1U << 1,
    POMODORO_OPENCODE_BIT_WORKING = 1U << 2,
    POMODORO_OPENCODE_BIT_COMPLETED = 1U << 3,
} pomodoro_opencode_bit_t;

typedef enum {
    POMODORO_OPENCODE_STATE_IDLE = 0,
    POMODORO_OPENCODE_STATE_WORKING,
    POMODORO_OPENCODE_STATE_WAITING,
    POMODORO_OPENCODE_STATE_COMPLETED,
    POMODORO_OPENCODE_STATE_ERROR,
    POMODORO_OPENCODE_STATE_HIDDEN,
} pomodoro_opencode_state_t;

typedef enum {
    POMODORO_OPENCODE_LINE_NONE = 0,
    POMODORO_OPENCODE_LINE_STATE,
    POMODORO_OPENCODE_LINE_INVALID,
} pomodoro_opencode_line_kind_t;

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
} pomodoro_opencode_status_t;

/* Parses one newline-terminated protocol line. STATE fills revision and mask.
 * Lines without the OC1 prefix are NONE; OC1 lines that are malformed, too
 * long, or use an unknown version or command are INVALID. */
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