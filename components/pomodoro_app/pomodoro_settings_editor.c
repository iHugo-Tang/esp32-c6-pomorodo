#include "pomodoro_settings_editor.h"

bool pomodoro_numeric_editor_open(
    pomodoro_numeric_editor_t *editor,
    const pomodoro_settings_t *settings,
    pomodoro_application_numeric_setting_t setting)
{
    if (editor == NULL || settings == NULL) {
        return false;
    }
    editor->setting = setting;
    editor->step = 1U;
    if (setting == POMODORO_APPLICATION_SETTING_FOCUS_MINUTES) {
        editor->value = settings->focus_minutes;
        editor->minimum = 5U;
        editor->maximum = 60U;
    } else if (setting == POMODORO_APPLICATION_SETTING_SHORT_BREAK_MINUTES) {
        editor->value = settings->short_break_minutes;
        editor->minimum = 1U;
        editor->maximum = 30U;
    } else if (setting == POMODORO_APPLICATION_SETTING_LONG_BREAK_MINUTES) {
        editor->value = settings->long_break_minutes;
        editor->minimum = 5U;
        editor->maximum = 60U;
    } else if (setting == POMODORO_APPLICATION_SETTING_DAILY_GOAL) {
        editor->value = settings->daily_goal;
        editor->minimum = 1U;
        editor->maximum = 16U;
    } else if (setting ==
               POMODORO_APPLICATION_SETTING_BRIGHTNESS_PERCENT) {
        editor->value = settings->brightness_percent;
        editor->minimum = 10U;
        editor->maximum = 100U;
        editor->step = 5U;
    } else {
        return false;
    }
    return true;
}

bool pomodoro_numeric_editor_adjust(pomodoro_numeric_editor_t *editor,
                                    int delta)
{
    if (editor == NULL || (delta != -1 && delta != 1)) {
        return false;
    }
    int adjusted = (int) editor->value + delta * (int) editor->step;
    if (adjusted >= (int) editor->minimum &&
        adjusted <= (int) editor->maximum) {
        editor->value = (uint16_t) adjusted;
    }
    return true;
}

void pomodoro_numeric_editor_apply(const pomodoro_numeric_editor_t *editor,
                                   pomodoro_settings_t *settings)
{
    if (editor->setting == POMODORO_APPLICATION_SETTING_FOCUS_MINUTES) {
        settings->focus_minutes = editor->value;
    } else if (editor->setting ==
               POMODORO_APPLICATION_SETTING_SHORT_BREAK_MINUTES) {
        settings->short_break_minutes = editor->value;
    } else if (editor->setting ==
               POMODORO_APPLICATION_SETTING_LONG_BREAK_MINUTES) {
        settings->long_break_minutes = editor->value;
    } else if (editor->setting == POMODORO_APPLICATION_SETTING_DAILY_GOAL) {
        settings->daily_goal = (uint8_t) editor->value;
    } else {
        settings->brightness_percent = (uint8_t) editor->value;
    }
}

bool pomodoro_manual_time_adjust(
    pomodoro_application_manual_time_t *draft,
    pomodoro_application_action_t action)
{
    if (draft == NULL) {
        return false;
    }
    int delta = action == POMODORO_APPLICATION_ACTION_MANUAL_YEAR_UP ||
            action == POMODORO_APPLICATION_ACTION_MANUAL_MONTH_UP ||
            action == POMODORO_APPLICATION_ACTION_MANUAL_DAY_UP ||
            action == POMODORO_APPLICATION_ACTION_MANUAL_HOUR_UP ||
            action == POMODORO_APPLICATION_ACTION_MANUAL_MINUTE_UP ||
            action == POMODORO_APPLICATION_ACTION_MANUAL_OFFSET_UP
        ? 1
        : -1;
    if (action == POMODORO_APPLICATION_ACTION_MANUAL_YEAR_DOWN ||
        action == POMODORO_APPLICATION_ACTION_MANUAL_YEAR_UP) {
        int value = (int) draft->local_date.year + delta;
        if (value >= 2020 && value <= 2069) {
            draft->local_date.year = (uint16_t) value;
        }
    } else if (action == POMODORO_APPLICATION_ACTION_MANUAL_MONTH_DOWN ||
               action == POMODORO_APPLICATION_ACTION_MANUAL_MONTH_UP) {
        int value = (int) draft->local_date.month + delta;
        draft->local_date.month = (uint8_t) (value < 1 ? 12 :
                                             value > 12 ? 1 : value);
    } else if (action == POMODORO_APPLICATION_ACTION_MANUAL_DAY_DOWN ||
               action == POMODORO_APPLICATION_ACTION_MANUAL_DAY_UP) {
        int value = (int) draft->local_date.day + delta;
        draft->local_date.day = (uint8_t) (value < 1 ? 31 :
                                           value > 31 ? 1 : value);
    } else if (action == POMODORO_APPLICATION_ACTION_MANUAL_HOUR_DOWN ||
               action == POMODORO_APPLICATION_ACTION_MANUAL_HOUR_UP) {
        int value = (int) draft->hour + delta;
        draft->hour = (uint8_t) (value < 0 ? 23 : value > 23 ? 0 : value);
    } else if (action == POMODORO_APPLICATION_ACTION_MANUAL_MINUTE_DOWN ||
               action == POMODORO_APPLICATION_ACTION_MANUAL_MINUTE_UP) {
        int value = (int) draft->minute + delta;
        draft->minute = (uint8_t) (value < 0 ? 59 : value > 59 ? 0 : value);
    } else if (action == POMODORO_APPLICATION_ACTION_MANUAL_OFFSET_DOWN ||
               action == POMODORO_APPLICATION_ACTION_MANUAL_OFFSET_UP) {
        int value = (int) draft->utc_offset_quarter_hours + delta;
        if (value >= -48 && value <= 56) {
            draft->utc_offset_quarter_hours = (int8_t) value;
        }
    } else {
        return false;
    }
    return true;
}
