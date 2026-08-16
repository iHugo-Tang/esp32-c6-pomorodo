#ifndef POMODORO_FOCUS_VIEW_H
#define POMODORO_FOCUS_VIEW_H

#include <stdbool.h>

#include "esp_err.h"
#include "pomodoro_application.h"

typedef void (*pomodoro_focus_action_handler_t)(
    pomodoro_application_action_t action);

esp_err_t pomodoro_focus_view_create(
    pomodoro_focus_action_handler_t action_handler);
bool pomodoro_focus_view_render(const pomodoro_application_view_t *view);
esp_err_t pomodoro_focus_view_start_ticks(void);
void pomodoro_focus_view_show_startup_error(const char *message,
                                            esp_err_t error);

#endif
