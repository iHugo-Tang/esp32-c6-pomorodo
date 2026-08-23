#ifndef POMODORO_OC1_PROTOCOL_H
#define POMODORO_OC1_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define POMODORO_OC1_MAX_LINE_LENGTH 64U
#define POMODORO_OC1_CONTROL_INTERVAL_SECONDS 3U

typedef enum {
    POMODORO_OC1_LINE_NONE = 0,
    POMODORO_OC1_LINE_STATE,
    POMODORO_OC1_LINE_INVALID,
} pomodoro_oc1_line_kind_t;

pomodoro_oc1_line_kind_t pomodoro_oc1_parse_line(
    const char *line, size_t length, uint32_t *revision, uint8_t *mask);
bool pomodoro_oc1_format_control(bool enabled, char *line, size_t size);
bool pomodoro_oc1_format_ack(uint32_t revision, uint8_t acknowledged_mask,
                             char *line, size_t size);

#ifdef __cplusplus
}
#endif

#endif
