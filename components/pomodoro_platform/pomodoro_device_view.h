#ifndef POMODORO_DEVICE_VIEW_H
#define POMODORO_DEVICE_VIEW_H

#include <stdbool.h>

#include "esp_err.h"
#include "pomodoro_application.h"

typedef void (*pomodoro_device_action_handler_t)(
    pomodoro_application_action_t action);
typedef void (*pomodoro_device_brightness_handler_t)(uint8_t percent,
                                                     bool confirmed);

esp_err_t pomodoro_device_view_create(
    pomodoro_device_action_handler_t action_handler,
    pomodoro_device_brightness_handler_t brightness_handler);
bool pomodoro_device_view_render(const pomodoro_application_view_t *view);
esp_err_t pomodoro_device_view_start_ticks(void);
void pomodoro_device_view_show_startup_error(const char *message,
                                             esp_err_t error);

#endif
