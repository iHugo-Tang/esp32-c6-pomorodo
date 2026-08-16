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
    destination->timer = model->timer;
    destination->statistics = *statistics;
    destination->checksum = state_checksum(destination);
    return true;
}

bool pomodoro_persistence_unpack(const pomodoro_persisted_state_t *source,
                                 pomodoro_model_t *model,
                                 pomodoro_statistics_store_t *statistics)
{
    if (source == NULL || model == NULL || statistics == NULL ||
        source->version != POMODORO_PERSISTED_STATE_VERSION ||
        source->settings.version != POMODORO_SETTINGS_VERSION ||
        source->timer.version != POMODORO_SNAPSHOT_VERSION ||
        source->statistics.version != POMODORO_STATISTICS_VERSION ||
        !pomodoro_settings_are_valid(&source->settings.value) ||
        source->checksum != state_checksum(source)) {
        return false;
    }

    if (pomodoro_init(model, &source->settings.value) != POMODORO_OK) {
        return false;
    }
    model->timer = source->timer;
    *statistics = source->statistics;
    return true;
}
