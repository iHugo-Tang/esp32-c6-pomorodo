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

bool pomodoro_persistence_pack(pomodoro_persisted_state_t *destination,
                               const pomodoro_model_t *model,
                               const pomodoro_statistics_store_t *statistics);
bool pomodoro_persistence_unpack(const pomodoro_persisted_state_t *source,
                                 pomodoro_model_t *model,
                                 pomodoro_statistics_store_t *statistics);

#ifdef __cplusplus
}
#endif

#endif
