#ifndef POMODORO_STATISTICS_PROJECTION_H
#define POMODORO_STATISTICS_PROJECTION_H

#include "pomodoro_application.h"

void pomodoro_statistics_project_view(
    const pomodoro_statistics_store_t *statistics,
    pomodoro_date_t today,
    uint16_t daily_goal,
    pomodoro_application_view_t *view);

#endif
