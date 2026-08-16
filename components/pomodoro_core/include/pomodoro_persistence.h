#ifndef POMODORO_PERSISTENCE_H
#define POMODORO_PERSISTENCE_H

#include <stdbool.h>
#include <stdint.h>

#include "pomodoro_core.h"
#include "pomodoro_statistics.h"

#ifdef __cplusplus
extern "C" {
#endif

#define POMODORO_PERSISTED_STATE_VERSION 1U

typedef struct {
    uint32_t version;
    pomodoro_persisted_settings_t settings;
    pomodoro_snapshot_t timer;
    pomodoro_statistics_store_t statistics;
    uint32_t checksum;
} pomodoro_persisted_state_t;

typedef struct {
    uint64_t generation;
    pomodoro_persisted_state_t state;
    uint32_t checksum;
} pomodoro_journal_slot_t;

typedef struct {
    void *context;
    bool (*read_slot)(void *context, uint8_t slot,
                      pomodoro_journal_slot_t *value);
    bool (*write_slot)(void *context, uint8_t slot,
                       const pomodoro_journal_slot_t *value);
} pomodoro_journal_io_t;

typedef struct {
    pomodoro_journal_io_t io;
    pomodoro_journal_slot_t workspace;
} pomodoro_journal_t;

bool pomodoro_persistence_pack(pomodoro_persisted_state_t *destination,
                               const pomodoro_model_t *model,
                               const pomodoro_statistics_store_t *statistics);
bool pomodoro_persistence_unpack(const pomodoro_persisted_state_t *source,
                                 pomodoro_model_t *model,
                                 pomodoro_statistics_store_t *statistics);
bool pomodoro_journal_load(pomodoro_journal_t *journal,
                           pomodoro_persisted_state_t *state);
bool pomodoro_journal_save(pomodoro_journal_t *journal,
                           const pomodoro_persisted_state_t *state);

#ifdef __cplusplus
}
#endif

#endif
