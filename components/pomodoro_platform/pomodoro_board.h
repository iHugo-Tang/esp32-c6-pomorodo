#ifndef POMODORO_BOARD_H
#define POMODORO_BOARD_H

#include <stdbool.h>

#include "esp_err.h"
#include "pomodoro_application.h"

esp_err_t pomodoro_board_initialize_i2c(void);
esp_err_t pomodoro_board_initialize_display(void);
esp_err_t pomodoro_board_initialize_touch(void);
esp_err_t pomodoro_board_initialize_rtc(void);
bool pomodoro_board_read_time(pomodoro_application_time_t *time);
bool pomodoro_board_resolve_local_date(uint64_t epoch_seconds,
                                       pomodoro_date_t *date);

#endif
