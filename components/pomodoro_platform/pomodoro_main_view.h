#ifndef POMODORO_MAIN_VIEW_H
#define POMODORO_MAIN_VIEW_H

#include <stdbool.h>

#include "esp_err.h"
#include "pomodoro_application.h"

typedef void (*pomodoro_main_action_handler_t)(
    pomodoro_application_action_t action);

esp_err_t pomodoro_main_view_create(
    pomodoro_main_action_handler_t action_handler);
bool pomodoro_main_view_render(const pomodoro_application_view_t *view);
esp_err_t pomodoro_main_view_start_ticks(void);
void pomodoro_main_view_show_startup_error(const char *message,
                                           esp_err_t error);

#endif
