#include "pomodoro_statistics.h"

#include <string.h>

static int compare_dates(pomodoro_date_t left, pomodoro_date_t right)
{
    if (left.year != right.year) {
        return left.year < right.year ? -1 : 1;
    }
    if (left.month != right.month) {
        return left.month < right.month ? -1 : 1;
    }
    if (left.day != right.day) {
        return left.day < right.day ? -1 : 1;
    }
    return 0;
}

static pomodoro_daily_statistics_t *find_mutable(
    pomodoro_statistics_store_t *store,
    pomodoro_date_t date)
{
    for (uint16_t index = 0; index < store->count; ++index) {
        if (compare_dates(store->days[index].date, date) == 0) {
            return &store->days[index];
        }
    }
    return NULL;
}

static pomodoro_daily_statistics_t *find_or_insert(
    pomodoro_statistics_store_t *store,
    pomodoro_date_t date)
{
    pomodoro_daily_statistics_t *existing = find_mutable(store, date);
    if (existing != NULL) {
        return existing;
    }

    uint16_t insertion = 0;
    while (insertion < store->count &&
           compare_dates(store->days[insertion].date, date) < 0) {
        insertion++;
    }

    if (store->count == POMODORO_STATISTICS_CAPACITY) {
        if (insertion == 0) {
            return NULL;
        }
        memmove(&store->days[0], &store->days[1],
                (POMODORO_STATISTICS_CAPACITY - 1U) * sizeof(store->days[0]));
        store->count--;
        insertion--;
    }

    memmove(&store->days[insertion + 1], &store->days[insertion],
            (store->count - insertion) * sizeof(store->days[0]));
    store->days[insertion] = (pomodoro_daily_statistics_t) {.date = date};
    store->count++;
    return &store->days[insertion];
}

void pomodoro_statistics_init(pomodoro_statistics_store_t *store)
{
    if (store == NULL) {
        return;
    }
    memset(store, 0, sizeof(*store));
    store->version = POMODORO_STATISTICS_VERSION;
}

bool pomodoro_date_is_valid(pomodoro_date_t date)
{
    static const uint8_t days_per_month[] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,
    };
    if (date.year < 1970 || date.month < 1 || date.month > 12 || date.day < 1) {
        return false;
    }
    uint8_t maximum = days_per_month[date.month - 1U];
    bool leap = (date.year % 4U == 0U && date.year % 100U != 0U) ||
                date.year % 400U == 0U;
    if (date.month == 2U && leap) {
        maximum++;
    }
    return date.day <= maximum;
}

bool pomodoro_statistics_apply(pomodoro_statistics_store_t *store,
                               const pomodoro_completion_record_t *completion,
                               bool rtc_is_valid,
                               pomodoro_date_t completion_date)
{
    if (store == NULL || completion == NULL ||
        store->version != POMODORO_STATISTICS_VERSION ||
        completion->settlement_sequence == 0U ||
        completion->settlement_sequence <= store->last_applied_settlement_sequence) {
        return false;
    }
    if (completion->mode == POMODORO_MODE_FOCUS && rtc_is_valid &&
        !pomodoro_date_is_valid(completion_date)) {
        return false;
    }

    store->last_applied_settlement_sequence = completion->settlement_sequence;
    if (completion->mode != POMODORO_MODE_FOCUS) {
        return true;
    }
    if (!rtc_is_valid) {
        store->pending_completed_pomodoros++;
        store->pending_focus_seconds += completion->configured_duration_seconds;
        return true;
    }
    pomodoro_daily_statistics_t *day = find_or_insert(store, completion_date);
    if (day == NULL) {
        return false;
    }
    day->completed_pomodoros++;
    day->completed_focus_seconds += completion->configured_duration_seconds;
    return true;
}

bool pomodoro_statistics_assign_pending(pomodoro_statistics_store_t *store,
                                        pomodoro_date_t restored_date)
{
    if (store == NULL || store->version != POMODORO_STATISTICS_VERSION ||
        !pomodoro_date_is_valid(restored_date)) {
        return false;
    }
    if (store->pending_completed_pomodoros == 0U) {
        return true;
    }

    pomodoro_daily_statistics_t *day = find_or_insert(store, restored_date);
    if (day == NULL) {
        return false;
    }
    day->completed_pomodoros += store->pending_completed_pomodoros;
    day->completed_focus_seconds += store->pending_focus_seconds;
    store->pending_completed_pomodoros = 0;
    store->pending_focus_seconds = 0;
    return true;
}

const pomodoro_daily_statistics_t *pomodoro_statistics_find(
    const pomodoro_statistics_store_t *store,
    pomodoro_date_t date)
{
    if (store == NULL) {
        return NULL;
    }
    for (uint16_t index = 0; index < store->count; ++index) {
        if (compare_dates(store->days[index].date, date) == 0) {
            return &store->days[index];
        }
    }
    return NULL;
}
