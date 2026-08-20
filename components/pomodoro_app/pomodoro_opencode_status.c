#include "pomodoro_opencode_status.h"

#include <stddef.h>

static uint8_t mask_bits(uint8_t mask)
{
    return mask & (POMODORO_OPENCODE_BIT_ERROR | POMODORO_OPENCODE_BIT_WAITING |
                   POMODORO_OPENCODE_BIT_WORKING |
                   POMODORO_OPENCODE_BIT_COMPLETED);
}

static bool token_equals(const char *line, size_t length, size_t offset,
                         const char *word)
{
    size_t word_length = 0U;
    while (word[word_length] != '\0') {
        word_length++;
    }
    if (offset + word_length > length) {
        return false;
    }
    for (size_t index = 0; index < word_length; index++) {
        if (line[offset + index] != word[index]) {
            return false;
        }
    }
    return (offset + word_length == length ||
            line[offset + word_length] == ' ');
}

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
    if (index < length && line[index] != ' ' && line[index] != '\0') {
        return false;
    }
    *value = (uint32_t) parsed;
    if (end != NULL) {
        *end = index;
    }
    return true;
}

pomodoro_opencode_line_kind_t pomodoro_opencode_parse_line(
    const char *line, size_t length, uint32_t *revision, uint8_t *mask)
{
    if (line == NULL || revision == NULL || mask == NULL ||
        length == 0U || length > POMODORO_OPENCODE_MAX_LINE_LENGTH) {
        return POMODORO_OPENCODE_LINE_NONE;
    }
    if (length < 4U || line[0] != 'O' || line[1] != 'C' || line[2] != '1' ||
        line[3] != ' ') {
        return POMODORO_OPENCODE_LINE_NONE;
    }
    size_t command_offset = 4U;
    size_t command_end = command_offset;
    while (command_end < length && line[command_end] != ' ') {
        command_end++;
    }
    size_t command_length = command_end - command_offset;
    if (command_length == 5U &&
        token_equals(line, command_end, command_offset, "STATE")) {
        size_t revision_offset = command_end + 1U;
        size_t revision_end = revision_offset;
        while (revision_end < length && line[revision_end] != ' ') {
            revision_end++;
        }
        uint32_t parsed_revision;
        if (!parse_unsigned(line, length, revision_offset, &parsed_revision,
                            NULL)) {
            return POMODORO_OPENCODE_LINE_INVALID;
        }
        size_t mask_offset = revision_end + 1U;
        size_t mask_end = mask_offset;
        uint32_t parsed_mask;
        if (revision_end == length || mask_offset >= length ||
            !parse_unsigned(line, length, mask_offset, &parsed_mask,
                            &mask_end) ||
            parsed_mask > 15U || mask_end != length) {
            return POMODORO_OPENCODE_LINE_INVALID;
        }
        *revision = parsed_revision;
        *mask = (uint8_t) parsed_mask;
        return POMODORO_OPENCODE_LINE_STATE;
    }
    return POMODORO_OPENCODE_LINE_INVALID;
}

void pomodoro_opencode_status_init(pomodoro_opencode_status_t *status,
                                   bool enabled,
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

void pomodoro_opencode_status_apply_state(
    pomodoro_opencode_status_t *status, uint32_t revision, uint8_t mask,
    uint64_t now_wall_time_seconds)
{
    if (status == NULL || !status->enabled) {
        return;
    }
    if (status->connected && status->has_revision &&
        revision < status->revision) {
        return;
    }
    uint8_t next_mask = mask_bits(mask);
    bool first_state = !status->has_revision;
    bool reconnecting = status->has_revision && !status->connected;
    bool revision_changed = status->has_revision &&
        revision != status->revision;
    bool mask_changed = status->has_revision &&
        next_mask != status->raw_mask;
    bool new_state_epoch = first_state || revision_changed ||
        (reconnecting && mask_changed);
    status->has_revision = true;
    status->connected = true;
    status->last_heartbeat_wall_time_seconds = now_wall_time_seconds;
    status->revision = revision;
    status->raw_mask = next_mask;
    if (new_state_epoch) {
        status->locally_cleared = 0U;
        if ((status->raw_mask & POMODORO_OPENCODE_BIT_COMPLETED) != 0U) {
            status->completed_deadline_wall_time_seconds =
                now_wall_time_seconds +
                POMODORO_OPENCODE_COMPLETED_DISPLAY_SECONDS;
        } else {
            status->completed_deadline_wall_time_seconds = 0U;
        }
        status->wake_pending = true;
    } else if ((status->raw_mask & POMODORO_OPENCODE_BIT_COMPLETED) == 0U) {
        status->completed_deadline_wall_time_seconds = 0U;
    }
}

void pomodoro_opencode_status_tick(pomodoro_opencode_status_t *status,
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
                POMODORO_OPENCODE_HEARTBEAT_TIMEOUT_SECONDS) {
        status->connected = false;
        status->completed_deadline_wall_time_seconds = 0U;
    }
}

void pomodoro_opencode_status_set_enabled(pomodoro_opencode_status_t *status,
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

bool pomodoro_opencode_status_ack(pomodoro_opencode_status_t *status,
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
    pomodoro_opencode_state_t state =
        pomodoro_opencode_status_resolve(status, now_wall_time_seconds);
    uint8_t bit = 0U;
    if (state == POMODORO_OPENCODE_STATE_ERROR) {
        bit = POMODORO_OPENCODE_BIT_ERROR;
    } else if (state == POMODORO_OPENCODE_STATE_COMPLETED) {
        bit = POMODORO_OPENCODE_BIT_COMPLETED;
    } else {
        return false;
    }
    status->locally_cleared = (uint8_t) (status->locally_cleared | bit);
    if (bit == POMODORO_OPENCODE_BIT_COMPLETED) {
        status->completed_deadline_wall_time_seconds = 0U;
    }
    *acknowledged_mask = bit;
    return true;
}

bool pomodoro_opencode_status_take_wake(pomodoro_opencode_status_t *status)
{
    if (status == NULL) {
        return false;
    }
    bool wake = status->wake_pending;
    status->wake_pending = false;
    return wake;
}

pomodoro_opencode_state_t pomodoro_opencode_status_resolve(
    const pomodoro_opencode_status_t *status,
    uint64_t now_wall_time_seconds)
{
    if (status == NULL || !status->enabled || !status->connected ||
        !status->has_revision) {
        return POMODORO_OPENCODE_STATE_HIDDEN;
    }
    uint8_t active =
        (uint8_t) (status->raw_mask & ~status->locally_cleared);
    if ((active & POMODORO_OPENCODE_BIT_ERROR) != 0U) {
        return POMODORO_OPENCODE_STATE_ERROR;
    }
    if ((active & POMODORO_OPENCODE_BIT_WAITING) != 0U) {
        return POMODORO_OPENCODE_STATE_WAITING;
    }
    if ((active & POMODORO_OPENCODE_BIT_WORKING) != 0U) {
        return POMODORO_OPENCODE_STATE_WORKING;
    }
    if ((active & POMODORO_OPENCODE_BIT_COMPLETED) != 0U &&
        status->completed_deadline_wall_time_seconds != 0U &&
        now_wall_time_seconds <
            status->completed_deadline_wall_time_seconds) {
        return POMODORO_OPENCODE_STATE_COMPLETED;
    }
    return POMODORO_OPENCODE_STATE_IDLE;
}

bool pomodoro_opencode_status_connected(
    const pomodoro_opencode_status_t *status)
{
    return status != NULL && status->enabled && status->connected;
}
