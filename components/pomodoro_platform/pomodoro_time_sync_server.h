#ifndef POMODORO_TIME_SYNC_SERVER_H
#define POMODORO_TIME_SYNC_SERVER_H

#include <stdbool.h>

#include "pomodoro_application.h"

bool pomodoro_time_sync_server_start(
    pomodoro_application_time_sync_session_t *session);
pomodoro_application_time_sync_poll_result_t
pomodoro_time_sync_server_poll(
    pomodoro_application_time_sync_submission_t *submission);
bool pomodoro_time_sync_server_stop(void);

#endif
