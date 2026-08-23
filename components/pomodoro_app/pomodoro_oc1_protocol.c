#include "pomodoro_oc1_protocol.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static bool parse_unsigned(const char *line, size_t length, size_t offset,
                           uint32_t *value, size_t *end)
{
    if (offset >= length || line[offset] < '0' || line[offset] > '9') {
        return false;
    }
    uint64_t parsed = 0U;
    size_t index = offset;
    while (index < length && line[index] >= '0' && line[index] <= '9') {
        parsed = parsed * 10U + (uint64_t) (line[index] - '0');
        if (parsed > UINT32_MAX) {
            return false;
        }
        index++;
    }
    if (index < length && line[index] != ' ') {
        return false;
    }
    *value = (uint32_t) parsed;
    if (end != NULL) {
        *end = index;
    }
    return true;
}

pomodoro_oc1_line_kind_t pomodoro_oc1_parse_line(
    const char *line, size_t length, uint32_t *revision, uint8_t *mask)
{
    static const char prefix[] = "OC1 ";
    static const char command[] = "STATE ";
    if (line == NULL || revision == NULL || mask == NULL || length == 0U ||
        length > POMODORO_OC1_MAX_LINE_LENGTH) {
        return POMODORO_OC1_LINE_NONE;
    }
    if (length < sizeof(prefix) - 1U ||
        memcmp(line, prefix, sizeof(prefix) - 1U) != 0) {
        return POMODORO_OC1_LINE_NONE;
    }
    size_t offset = sizeof(prefix) - 1U;
    if (length < offset + sizeof(command) - 1U ||
        memcmp(line + offset, command, sizeof(command) - 1U) != 0) {
        return POMODORO_OC1_LINE_INVALID;
    }
    offset += sizeof(command) - 1U;
    size_t revision_end = offset;
    uint32_t parsed_revision = 0U;
    uint32_t parsed_mask = 0U;
    size_t mask_end = 0U;
    if (!parse_unsigned(line, length, offset, &parsed_revision,
                        &revision_end) ||
        revision_end >= length || line[revision_end] != ' ' ||
        !parse_unsigned(line, length, revision_end + 1U, &parsed_mask,
                        &mask_end) ||
        parsed_mask > 15U || mask_end != length) {
        return POMODORO_OC1_LINE_INVALID;
    }
    *revision = parsed_revision;
    *mask = (uint8_t) parsed_mask;
    return POMODORO_OC1_LINE_STATE;
}

bool pomodoro_oc1_format_control(bool enabled, char *line, size_t size)
{
    if (line == NULL || size == 0U) {
        return false;
    }
    int written = snprintf(line, size, "OC1 %s",
                           enabled ? "ENABLED" : "DISABLED");
    return written >= 0 && (size_t) written < size;
}

bool pomodoro_oc1_format_ack(uint32_t revision, uint8_t acknowledged_mask,
                             char *line, size_t size)
{
    if (line == NULL || size == 0U ||
        (acknowledged_mask != 1U && acknowledged_mask != 8U)) {
        return false;
    }
    int written = snprintf(line, size, "OC1 ACK %" PRIu32 " %u", revision,
                           (unsigned int) acknowledged_mask);
    return written >= 0 && (size_t) written < size;
}
