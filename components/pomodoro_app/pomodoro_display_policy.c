#include "pomodoro_display_policy.h"

#include <stddef.h>

pomodoro_display_policy_result_t pomodoro_display_policy_resolve(
    pomodoro_timer_state_t timer_state, uint64_t idle_seconds,
    uint8_t chosen_brightness_percent, uint8_t idle_brightness_percent)
{
    pomodoro_display_policy_result_t result = {
        .awake = true,
        .brightness_percent = chosen_brightness_percent,
    };
    if (idle_seconds < 120U) {
        return result;
    }
    if (timer_state == POMODORO_TIMER_RUNNING) {
        result.brightness_percent = (uint8_t) (
            (uint16_t) chosen_brightness_percent * 60U / 100U);
        return result;
    }
    if (idle_seconds >= 300U) {
        result.awake = false;
        result.brightness_percent = 0U;
        return result;
    }
    result.brightness_percent = (uint8_t) (
        (uint16_t) chosen_brightness_percent * idle_brightness_percent / 100U);
    return result;
}

void pomodoro_display_policy_pixel_shift(uint64_t wall_time_seconds,
                                         int8_t *x, int8_t *y)
{
    if (x == NULL || y == NULL) {
        return;
    }
    uint8_t shift = (uint8_t) ((wall_time_seconds / 60U) % 5U);
    *x = (int8_t) shift - 2;
    *y = 2 - (int8_t) shift;
}
