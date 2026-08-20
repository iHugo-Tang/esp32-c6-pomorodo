#include "pomodoro_persistence.h"

#include <stddef.h>
#include <string.h>

static uint32_t crc32(const void *data, size_t size)
{
    const uint8_t *bytes = data;
    uint32_t crc = UINT32_C(0xFFFFFFFF);
    for (size_t index = 0; index < size; ++index) {
        crc ^= bytes[index];
        for (unsigned bit = 0; bit < 8U; ++bit) {
            uint32_t mask = (uint32_t) -(int32_t) (crc & 1U);
            crc = (crc >> 1U) ^ (UINT32_C(0xEDB88320) & mask);
        }
    }
    return ~crc;
}

static uint32_t state_checksum(const pomodoro_persisted_state_t *state)
{
    return crc32(state, offsetof(pomodoro_persisted_state_t, checksum));
}

static bool state_is_valid(const pomodoro_persisted_state_t *state)
{
    return state != NULL &&
           state->version == POMODORO_PERSISTED_STATE_VERSION &&
           state->settings.version == POMODORO_SETTINGS_VERSION &&
           state->timer.version == POMODORO_SNAPSHOT_VERSION &&
           state->statistics.version == POMODORO_STATISTICS_VERSION &&
           pomodoro_settings_are_valid(&state->settings.value) &&
           state->settings.last_nonzero_volume >= 1U &&
           state->settings.last_nonzero_volume <= 5U &&
           state->checksum == state_checksum(state);
}

static uint32_t slot_checksum(const pomodoro_journal_slot_t *slot)
{
    return crc32(slot, offsetof(pomodoro_journal_slot_t, checksum));
}

static bool slot_is_valid(const pomodoro_journal_slot_t *slot)
{
    return slot != NULL && slot->generation != 0U &&
           slot->checksum == slot_checksum(slot) &&
           state_is_valid(&slot->state);
}

bool pomodoro_persistence_pack(pomodoro_persisted_state_t *destination,
                               const pomodoro_model_t *model,
                               const pomodoro_statistics_store_t *statistics)
{
    if (destination == NULL || model == NULL || statistics == NULL ||
        !pomodoro_settings_are_valid(&model->settings) ||
        model->timer.version != POMODORO_SNAPSHOT_VERSION ||
        statistics->version != POMODORO_STATISTICS_VERSION) {
        return false;
    }

    memset(destination, 0, sizeof(*destination));
    destination->version = POMODORO_PERSISTED_STATE_VERSION;
    destination->settings.version = POMODORO_SETTINGS_VERSION;
    destination->settings.value = model->settings;
    destination->settings.last_nonzero_volume = model->last_nonzero_volume;
    destination->timer = model->timer;
    destination->statistics = *statistics;
    destination->checksum = state_checksum(destination);
    return true;
}

bool pomodoro_persistence_migrate(pomodoro_persisted_state_t *state)
{
    return state_is_valid(state);
}

bool pomodoro_persistence_unpack(pomodoro_persisted_state_t *source,
                                 pomodoro_model_t *model,
                                 pomodoro_statistics_store_t *statistics,
                                 bool *was_migrated)
{
    bool current_schema = state_is_valid(source);
    if (was_migrated != NULL) {
        *was_migrated = false;
    }
    if (model == NULL || statistics == NULL ||
        !pomodoro_persistence_migrate(source)) {
        return false;
    }

    if (pomodoro_init(model, &source->settings.value) != POMODORO_OK) {
        return false;
    }
    model->timer = source->timer;
    model->last_nonzero_volume = source->settings.last_nonzero_volume;
    if (model->last_nonzero_volume == 0U ||
        model->last_nonzero_volume > 5U) {
        model->last_nonzero_volume = model->settings.volume > 0U
            ? model->settings.volume
            : 3U;
    }
    *statistics = source->statistics;
    if (was_migrated != NULL) {
        *was_migrated = !current_schema;
    }
    return true;
}

pomodoro_journal_read_result_t pomodoro_journal_load(
    pomodoro_journal_t *journal, pomodoro_persisted_state_t *state)
{
    if (journal == NULL || state == NULL || journal->io.read_slot == NULL) {
        return POMODORO_JOURNAL_READ_ERROR;
    }

    bool found = false;
    uint64_t selected_generation = 0;
    for (uint8_t index = 0; index < 2U; ++index) {
        memset(&journal->workspace, 0, sizeof(journal->workspace));
        pomodoro_journal_read_result_t read = journal->io.read_slot(
            journal->io.context, index, &journal->workspace);
        if (read == POMODORO_JOURNAL_READ_ERROR) {
            return POMODORO_JOURNAL_READ_ERROR;
        }
        uint64_t generation = 0U;
        bool valid = read == POMODORO_JOURNAL_READ_LOADED &&
            slot_is_valid(&journal->workspace) &&
            (journal->workspace.generation > 0U);
        if (valid) {
            generation = journal->workspace.generation;
        }
        if (valid && (!found || generation > selected_generation)) {
            *state = journal->workspace.state;
            selected_generation = generation;
            found = true;
        }
    }
    return found ? POMODORO_JOURNAL_READ_LOADED
                 : POMODORO_JOURNAL_READ_EMPTY;
}

bool pomodoro_journal_save(pomodoro_journal_t *journal,
                           const pomodoro_persisted_state_t *state)
{
    if (journal == NULL || !state_is_valid(state) ||
        journal->io.read_slot == NULL || journal->io.write_slot == NULL) {
        return false;
    }

    bool valid[2] = {false, false};
    uint64_t generations[2] = {0, 0};
    for (uint8_t index = 0; index < 2U; ++index) {
        memset(&journal->workspace, 0, sizeof(journal->workspace));
        pomodoro_journal_read_result_t read = journal->io.read_slot(
            journal->io.context, index, &journal->workspace);
        if (read == POMODORO_JOURNAL_READ_ERROR) {
            return false;
        }
        valid[index] = read == POMODORO_JOURNAL_READ_LOADED &&
            slot_is_valid(&journal->workspace);
        if (valid[index]) {
            generations[index] = journal->workspace.generation;
        }
    }

    uint8_t target = 0;
    uint64_t generation = 1;
    if (valid[0] || valid[1]) {
        uint8_t current = valid[1] &&
                          (!valid[0] || generations[1] > generations[0])
            ? 1U
            : 0U;
        target = current == 0U ? 1U : 0U;
        generation = generations[current] + 1U;
    }

    memset(&journal->workspace, 0, sizeof(journal->workspace));
    journal->workspace.generation = generation;
    journal->workspace.state = *state;
    journal->workspace.checksum = slot_checksum(&journal->workspace);
    if (!journal->io.write_slot(journal->io.context, target,
                                &journal->workspace)) {
        return false;
    }

    memset(&journal->workspace, 0, sizeof(journal->workspace));
    return journal->io.read_slot(journal->io.context, target,
                                 &journal->workspace) ==
               POMODORO_JOURNAL_READ_LOADED &&
           slot_is_valid(&journal->workspace) &&
           journal->workspace.generation == generation &&
           journal->workspace.state.checksum == state->checksum;
}
