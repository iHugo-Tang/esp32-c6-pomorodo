#ifndef POMODORO_DISPLAY_POLICY_H
#define POMODORO_DISPLAY_POLICY_H

#include <stdbool.h>
#include <stdint.h>

#include "pomodoro_core.h"

typedef struct {
    bool awake;
    uint8_t brightness_percent;
} pomodoro_display_policy_result_t;

pomodoro_display_policy_result_t pomodoro_display_policy_resolve(
    pomodoro_timer_state_t timer_state, uint64_t idle_seconds,
    uint8_t chosen_brightness_percent, uint8_t idle_brightness_percent);
void pomodoro_display_policy_pixel_shift(uint64_t wall_time_seconds,
                                         int8_t *x, int8_t *y);

#endif
