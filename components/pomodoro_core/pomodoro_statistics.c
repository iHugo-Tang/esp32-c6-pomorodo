#include "pomodoro_statistics.h"

#include <string.h>

static int64_t days_since_unix_epoch(int32_t year, uint8_t month, uint8_t day)
{
    year -= month <= 2U;
    int32_t era = (year >= 0 ? year : year - 399) / 400;
    uint32_t year_of_era = (uint32_t) (year - era * 400);
    uint32_t day_of_year =
        (153U * (uint32_t) (month + (month > 2U ? -3 : 9)) + 2U) / 5U +
        day - 1U;
    uint32_t day_of_era = year_of_era * 365U + year_of_era / 4U -
        year_of_era / 100U + day_of_year;
    return (int64_t) era * 146097 + (int64_t) day_of_era - 719468;
}

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

bool pomodoro_date_add_days(pomodoro_date_t date, int days,
                            pomodoro_date_t *result)
{
    if (!pomodoro_date_is_valid(date) || result == NULL) {
        return false;
    }
    int64_t resolved = days_since_unix_epoch(
        date.year, date.month, date.day) + days;
    if (resolved < 0 || (uint64_t) resolved > UINT64_MAX / 86400U) {
        return false;
    }
    return pomodoro_local_date_from_wall_time(
        (uint64_t) resolved * 86400U, result);
}

bool pomodoro_local_datetime_to_wall_time(
    pomodoro_date_t date, uint8_t hour, uint8_t minute,
    uint64_t *wall_time_seconds)
{
    if (!pomodoro_date_is_valid(date) || hour > 23U || minute > 59U ||
        wall_time_seconds == NULL) {
        return false;
    }
    int64_t local_seconds = days_since_unix_epoch(
        date.year, date.month, date.day) * 86400 +
        (int64_t) hour * 3600 + (int64_t) minute * 60;
    if (local_seconds < 0) {
        return false;
    }
    *wall_time_seconds = (uint64_t) local_seconds;
    return true;
}

bool pomodoro_local_date_from_wall_time(uint64_t wall_time_seconds,
                                    pomodoro_date_t *date)
{
    if (date == NULL || wall_time_seconds > INT64_MAX) {
        return false;
    }
    int64_t local_seconds = (int64_t) wall_time_seconds;
    int64_t days = local_seconds / 86400;
    if (local_seconds < 0 && local_seconds % 86400 != 0) {
        days--;
    }
    days += 719468;
    int64_t era = (days >= 0 ? days : days - 146096) / 146097;
    uint32_t day_of_era = (uint32_t) (days - era * 146097);
    uint32_t year_of_era =
        (day_of_era - day_of_era / 1460U + day_of_era / 36524U -
         day_of_era / 146096U) / 365U;
    int64_t year = (int64_t) year_of_era + era * 400;
    uint32_t day_of_year = day_of_era -
        (365U * year_of_era + year_of_era / 4U - year_of_era / 100U);
    uint32_t month_part = (5U * day_of_year + 2U) / 153U;
    uint8_t resolved_day = (uint8_t) (day_of_year -
        (153U * month_part + 2U) / 5U + 1U);
    uint8_t month = (uint8_t) ((int32_t) month_part +
        (month_part < 10U ? 3 : -9));
    year += month <= 2U;
    if (year < 0 || year > UINT16_MAX) {
        return false;
    }
    *date = (pomodoro_date_t) {
        .year = (uint16_t) year,
        .month = month,
        .day = resolved_day,
    };
    return pomodoro_date_is_valid(*date);
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

    if (completion->mode == POMODORO_MODE_FOCUS) {
        if (!rtc_is_valid) {
            store->pending_completed_pomodoros++;
            store->pending_focus_seconds +=
                completion->configured_duration_seconds;
        } else {
            pomodoro_daily_statistics_t *day =
                find_or_insert(store, completion_date);
            if (day == NULL) {
                return false;
            }
            day->completed_pomodoros++;
            day->completed_focus_seconds +=
                completion->configured_duration_seconds;
        }
    }
    store->last_applied_settlement_sequence = completion->settlement_sequence;
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
