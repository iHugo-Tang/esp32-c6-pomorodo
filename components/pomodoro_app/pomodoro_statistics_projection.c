#include "pomodoro_statistics_projection.h"

#include <limits.h>
#include <string.h>

void pomodoro_statistics_project_view(
    const pomodoro_statistics_store_t *statistics,
    pomodoro_date_t today,
    uint16_t daily_goal,
    pomodoro_application_view_t *view)
{
    view->today_completed_pomodoros = 0U;
    view->today_completed_focus_seconds = 0U;
    view->today_goal_percent = 0U;
    view->today_goal_fill_percent = 0U;
    view->statistics_chart_scale = daily_goal;
    memset(view->trailing_seven_days, 0, sizeof(view->trailing_seven_days));

    const pomodoro_daily_statistics_t *today_statistics =
        pomodoro_statistics_find(statistics, today);
    if (today_statistics != NULL) {
        view->today_completed_pomodoros =
            today_statistics->completed_pomodoros;
        view->today_completed_focus_seconds =
            today_statistics->completed_focus_seconds;
    }
    uint32_t percent =
        (uint32_t) view->today_completed_pomodoros * 100U / daily_goal;
    view->today_goal_percent = percent > UINT16_MAX
        ? UINT16_MAX
        : (uint16_t) percent;
    view->today_goal_fill_percent = percent > 100U
        ? 100U
        : (uint8_t) percent;

    for (int offset = -6; offset <= 0; ++offset) {
        pomodoro_date_t date;
        if (!pomodoro_date_add_days(today, offset, &date)) {
            continue;
        }
        size_t index = (size_t) (offset + 6);
        const pomodoro_daily_statistics_t *day =
            pomodoro_statistics_find(statistics, date);
        view->trailing_seven_days[index] =
            (pomodoro_application_statistics_bar_t) {
                .date = date,
                .completed_pomodoros = day != NULL
                    ? day->completed_pomodoros
                    : 0U,
                .is_today = offset == 0,
            };
        if (day != NULL &&
            day->completed_pomodoros > view->statistics_chart_scale) {
            view->statistics_chart_scale = day->completed_pomodoros;
        }
    }
}
