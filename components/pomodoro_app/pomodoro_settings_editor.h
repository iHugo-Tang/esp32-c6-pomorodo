#ifndef POMODORO_SETTINGS_EDITOR_H
#define POMODORO_SETTINGS_EDITOR_H

#include "pomodoro_application.h"

typedef struct {
    pomodoro_application_numeric_setting_t setting;
    uint16_t value;
    uint16_t minimum;
    uint16_t maximum;
    uint16_t step;
} pomodoro_numeric_editor_t;

bool pomodoro_numeric_editor_open(
    pomodoro_numeric_editor_t *editor,
    const pomodoro_settings_t *settings,
    pomodoro_application_numeric_setting_t setting);
bool pomodoro_numeric_editor_adjust(pomodoro_numeric_editor_t *editor,
                                    int delta);
void pomodoro_numeric_editor_apply(const pomodoro_numeric_editor_t *editor,
                                   pomodoro_settings_t *settings);
bool pomodoro_manual_time_adjust(
    pomodoro_application_manual_time_t *draft,
    pomodoro_application_action_t action);

#endif
