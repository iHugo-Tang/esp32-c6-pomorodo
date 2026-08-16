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

typedef struct {
    uint16_t focus_minutes;
    uint16_t short_break_minutes;
    uint16_t long_break_minutes;
    uint8_t daily_goal;
    bool auto_start_breaks;
    bool auto_start_focus;
    uint8_t volume;
    uint8_t brightness_percent;
} legacy_v1_settings_t;

typedef struct {
    uint32_t version;
    legacy_v1_settings_t value;
} legacy_v1_persisted_settings_t;

typedef struct {
    uint32_t version;
    legacy_v1_persisted_settings_t settings;
    pomodoro_snapshot_t timer;
    pomodoro_statistics_store_t statistics;
    uint32_t checksum;
} legacy_v1_persisted_state_t;

typedef struct {
    uint16_t focus_minutes;
    uint16_t short_break_minutes;
    uint16_t long_break_minutes;
    uint8_t daily_goal;
    bool auto_start_breaks;
    bool auto_start_focus;
    uint8_t volume;
    uint8_t brightness_percent;
    int8_t utc_offset_quarter_hours;
} legacy_v2_settings_t;

typedef struct {
    uint32_t version;
    legacy_v2_settings_t value;
    uint8_t last_nonzero_volume;
} legacy_v2_persisted_settings_t;

typedef struct {
    uint32_t version;
    legacy_v2_persisted_settings_t settings;
    pomodoro_snapshot_t timer;
    pomodoro_statistics_store_t statistics;
    uint32_t checksum;
} legacy_v2_persisted_state_t;

typedef struct {
    uint64_t generation;
    legacy_v1_persisted_state_t state;
    uint32_t checksum;
} legacy_v1_journal_slot_t;

typedef struct {
    uint64_t generation;
    legacy_v2_persisted_state_t state;
    uint32_t checksum;
} legacy_v2_journal_slot_t;

typedef struct {
    uint32_t version;
    legacy_v2_settings_t value;
    uint8_t last_nonzero_volume;
    pomodoro_time_zone_schedule_t time_zone;
} legacy_v3_persisted_settings_t;

typedef struct {
    uint32_t version;
    legacy_v3_persisted_settings_t settings;
    pomodoro_snapshot_t timer;
    pomodoro_statistics_store_t statistics;
    uint32_t checksum;
} legacy_v3_persisted_state_t;

typedef struct {
    uint64_t generation;
    legacy_v3_persisted_state_t state;
    uint32_t checksum;
} legacy_v3_journal_slot_t;

static pomodoro_settings_t migrate_pre_v4_settings(
    const legacy_v2_settings_t *legacy)
{
    return (pomodoro_settings_t) {
        .focus_minutes = legacy->focus_minutes,
        .short_break_minutes = legacy->short_break_minutes,
        .long_break_minutes = legacy->long_break_minutes,
        .daily_goal = legacy->daily_goal,
        .auto_start_breaks = legacy->auto_start_breaks,
        .auto_start_focus = legacy->auto_start_focus,
        .volume = legacy->volume,
        .brightness_percent = legacy->brightness_percent,
        .idle_brightness_percent = 20U,
        .utc_offset_quarter_hours = legacy->utc_offset_quarter_hours,
    };
}

static bool state_is_valid(const pomodoro_persisted_state_t *state)
{
    return state != NULL &&
           state->version == POMODORO_PERSISTED_STATE_VERSION &&
           state->settings.version == POMODORO_SETTINGS_VERSION &&
           state->timer.version == POMODORO_SNAPSHOT_VERSION &&
           state->statistics.version == POMODORO_STATISTICS_VERSION &&
           pomodoro_settings_are_valid(&state->settings.value) &&
           pomodoro_time_zone_schedule_is_valid(
               &state->settings.time_zone) &&
           state->settings.last_nonzero_volume >= 1U &&
           state->settings.last_nonzero_volume <= 5U &&
           state->checksum == state_checksum(state);
}

static bool legacy_v1_state_is_valid(
    const legacy_v1_persisted_state_t *state)
{
    if (state == NULL || (state->version != 0U && state->version != 1U) ||
        state->settings.version != state->version ||
        (state->timer.version != 0U &&
         state->timer.version != POMODORO_SNAPSHOT_VERSION) ||
        (state->statistics.version != 0U &&
         state->statistics.version != POMODORO_STATISTICS_VERSION)) {
        return false;
    }
    pomodoro_settings_t settings = {
        .focus_minutes = state->settings.value.focus_minutes,
        .short_break_minutes = state->settings.value.short_break_minutes,
        .long_break_minutes = state->settings.value.long_break_minutes,
        .daily_goal = state->settings.value.daily_goal,
        .auto_start_breaks = state->settings.value.auto_start_breaks,
        .auto_start_focus = state->settings.value.auto_start_focus,
        .volume = state->settings.value.volume,
        .brightness_percent = state->settings.value.brightness_percent,
        .idle_brightness_percent = 20U,
        .utc_offset_quarter_hours = 0,
    };
    return
           pomodoro_settings_are_valid(&settings) &&
           state->checksum == crc32(
               state, offsetof(legacy_v1_persisted_state_t, checksum));
}

static bool legacy_v2_state_is_valid(
    const legacy_v2_persisted_state_t *state)
{
    pomodoro_settings_t settings = state != NULL
        ? migrate_pre_v4_settings(&state->settings.value)
        : pomodoro_default_settings();
    return state != NULL && state->version == 2U &&
           state->settings.version == 2U &&
           state->timer.version == POMODORO_SNAPSHOT_VERSION &&
           state->statistics.version == POMODORO_STATISTICS_VERSION &&
           pomodoro_settings_are_valid(&settings) &&
           state->settings.last_nonzero_volume >= 1U &&
           state->settings.last_nonzero_volume <= 5U &&
           state->checksum == crc32(
               state, offsetof(legacy_v2_persisted_state_t, checksum));
}

static bool legacy_v3_state_is_valid(
    const legacy_v3_persisted_state_t *state)
{
    pomodoro_settings_t settings = state != NULL
        ? migrate_pre_v4_settings(&state->settings.value)
        : pomodoro_default_settings();
    return state != NULL && state->version == 3U &&
           state->settings.version == 3U &&
           state->timer.version == POMODORO_SNAPSHOT_VERSION &&
           state->statistics.version == POMODORO_STATISTICS_VERSION &&
           pomodoro_settings_are_valid(&settings) &&
           pomodoro_time_zone_schedule_is_valid(
               &state->settings.time_zone) &&
           state->settings.last_nonzero_volume >= 1U &&
           state->settings.last_nonzero_volume <= 5U &&
           state->checksum == crc32(
               state, offsetof(legacy_v3_persisted_state_t, checksum));
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

static bool migrate_legacy_state(const void *source,
                                 pomodoro_persisted_state_t *destination)
{
    const legacy_v1_persisted_state_t *v1 = source;
    const legacy_v2_persisted_state_t *v2 = source;
    const legacy_v3_persisted_state_t *v3 = source;
    pomodoro_settings_t settings;
    uint8_t last_nonzero_volume;
    pomodoro_time_zone_schedule_t time_zone;
    const pomodoro_snapshot_t *timer;
    const pomodoro_statistics_store_t *statistics;
    memset(&time_zone, 0, sizeof(time_zone));
    if (legacy_v3_state_is_valid(v3)) {
        settings = migrate_pre_v4_settings(&v3->settings.value);
        last_nonzero_volume = v3->settings.last_nonzero_volume;
        time_zone = v3->settings.time_zone;
        timer = &v3->timer;
        statistics = &v3->statistics;
    } else if (legacy_v2_state_is_valid(v2)) {
        settings = migrate_pre_v4_settings(&v2->settings.value);
        last_nonzero_volume = v2->settings.last_nonzero_volume;
        timer = &v2->timer;
        statistics = &v2->statistics;
    } else if (legacy_v1_state_is_valid(v1)) {
        settings = (pomodoro_settings_t) {
            .focus_minutes = v1->settings.value.focus_minutes,
            .short_break_minutes = v1->settings.value.short_break_minutes,
            .long_break_minutes = v1->settings.value.long_break_minutes,
            .daily_goal = v1->settings.value.daily_goal,
            .auto_start_breaks = v1->settings.value.auto_start_breaks,
            .auto_start_focus = v1->settings.value.auto_start_focus,
            .volume = v1->settings.value.volume,
            .brightness_percent = v1->settings.value.brightness_percent,
            .idle_brightness_percent = 20U,
            .utc_offset_quarter_hours = 0,
        };
        last_nonzero_volume =
            v1->settings.value.volume > 0U ? v1->settings.value.volume : 3U;
        timer = &v1->timer;
        statistics = &v1->statistics;
    } else {
        return false;
    }
    if (source != destination) {
        memset(destination, 0, sizeof(*destination));
    }
    memmove(&destination->statistics, statistics, sizeof(*statistics));
    memmove(&destination->timer, timer, sizeof(*timer));
    memset(&destination->settings, 0, sizeof(destination->settings));
    destination->settings.value = settings;
    destination->settings.last_nonzero_volume = last_nonzero_volume;
    destination->settings.time_zone = time_zone;
    destination->version = POMODORO_PERSISTED_STATE_VERSION;
    destination->settings.version = POMODORO_SETTINGS_VERSION;
    destination->timer.version = POMODORO_SNAPSHOT_VERSION;
    destination->statistics.version = POMODORO_STATISTICS_VERSION;
    destination->checksum = state_checksum(destination);
    return true;
}

static bool loadable_slot(const pomodoro_journal_slot_t *slot,
                          pomodoro_persisted_state_t *state,
                          uint64_t *generation)
{
    if (slot_is_valid(slot)) {
        *state = slot->state;
        *generation = slot->generation;
        return true;
    }
    const legacy_v3_journal_slot_t *v3 = (const void *) slot;
    if (v3->generation != 0U && legacy_v3_state_is_valid(&v3->state) &&
        v3->checksum == crc32(
            v3, offsetof(legacy_v3_journal_slot_t, checksum)) &&
        migrate_legacy_state(&v3->state, state)) {
        *generation = v3->generation;
        return true;
    }
    const legacy_v2_journal_slot_t *v2 = (const void *) slot;
    if (v2->generation != 0U && legacy_v2_state_is_valid(&v2->state) &&
        v2->checksum == crc32(
            v2, offsetof(legacy_v2_journal_slot_t, checksum)) &&
        migrate_legacy_state(&v2->state, state)) {
        *generation = v2->generation;
        return true;
    }
    const legacy_v1_journal_slot_t *v1 = (const void *) slot;
    if (v1->generation != 0U && legacy_v1_state_is_valid(&v1->state) &&
        v1->checksum == crc32(
            v1, offsetof(legacy_v1_journal_slot_t, checksum)) &&
        migrate_legacy_state(&v1->state, state)) {
        *generation = v1->generation;
        return true;
    }
    return false;
}

bool pomodoro_persistence_pack(pomodoro_persisted_state_t *destination,
                               const pomodoro_model_t *model,
                               const pomodoro_statistics_store_t *statistics)
{
    if (destination == NULL || model == NULL || statistics == NULL ||
        !pomodoro_settings_are_valid(&model->settings) ||
        !pomodoro_time_zone_schedule_is_valid(&model->time_zone) ||
        model->timer.version != POMODORO_SNAPSHOT_VERSION ||
        statistics->version != POMODORO_STATISTICS_VERSION) {
        return false;
    }

    memset(destination, 0, sizeof(*destination));
    destination->version = POMODORO_PERSISTED_STATE_VERSION;
    destination->settings.version = POMODORO_SETTINGS_VERSION;
    destination->settings.value = model->settings;
    destination->settings.last_nonzero_volume = model->last_nonzero_volume;
    destination->settings.time_zone = model->time_zone;
    destination->timer = model->timer;
    destination->statistics = *statistics;
    destination->checksum = state_checksum(destination);
    return true;
}

bool pomodoro_persistence_migrate(pomodoro_persisted_state_t *state)
{
    if (state_is_valid(state)) {
        return true;
    }
    return migrate_legacy_state(state, state);
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
    model->time_zone = source->settings.time_zone;
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
            loadable_slot(&journal->workspace, &journal->candidate,
                          &generation);
        if (valid && (!found || generation > selected_generation)) {
            *state = journal->candidate;
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
            loadable_slot(&journal->workspace, &journal->candidate,
                          &generations[index]);
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
