#ifndef POMODORO_STATISTICS_H
#define POMODORO_STATISTICS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "pomodoro_core.h"

#ifdef __cplusplus
extern "C" {
#endif

#define POMODORO_STATISTICS_VERSION 1U
#define POMODORO_STATISTICS_CAPACITY 365U

typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
} pomodoro_date_t;

typedef struct {
    pomodoro_date_t date;
    uint16_t completed_pomodoros;
    uint32_t completed_focus_seconds;
} pomodoro_daily_statistics_t;

typedef struct {
    uint32_t version;
    uint32_t last_applied_settlement_sequence;
    uint16_t count;
    uint16_t pending_completed_pomodoros;
    uint32_t pending_focus_seconds;
    pomodoro_daily_statistics_t days[POMODORO_STATISTICS_CAPACITY];
} pomodoro_statistics_store_t;

void pomodoro_statistics_init(pomodoro_statistics_store_t *store);
bool pomodoro_date_is_valid(pomodoro_date_t date);
bool pomodoro_statistics_apply(pomodoro_statistics_store_t *store,
                               const pomodoro_completion_record_t *completion,
                               bool rtc_is_valid,
                               pomodoro_date_t completion_date);
bool pomodoro_statistics_assign_pending(pomodoro_statistics_store_t *store,
                                        pomodoro_date_t restored_date);
const pomodoro_daily_statistics_t *pomodoro_statistics_find(
    const pomodoro_statistics_store_t *store,
    pomodoro_date_t date);

#ifdef __cplusplus
}
#endif

#endif
