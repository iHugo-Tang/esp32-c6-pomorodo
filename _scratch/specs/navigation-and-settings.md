# Navigation and settings

## Navigation

Main uses an app bar with Menu on the left, the current Mode in the center, and battery status on the right. Menu opens a dedicated page containing Statistics, Settings, and `Clear Set Progress`.

Statistics and Settings opened from Menu return to Menu. Back from Menu returns to Main. A blocking confirmation dialog prevents other navigation until the user confirms or cancels.

## Settings

Settings covers Pomodoro durations, auto-start behavior, sound, display, local date and time, Daily Goal, Agent Status, Waiting Sound, and power actions. Changes follow one of three patterns:

- switches apply and persist immediately;
- numeric values are edited as drafts and persist only on confirmation;
- brightness previews immediately, then confirmation keeps it or cancellation restores the previous value.

Editing Settings never changes an active Timer except where a requirement explicitly says so. Date and time correction preserves the Timer's remaining duration as described in [Date and time](date-and-time.md). Sound and Agent settings follow their own domain rules.

## Switch rows

A switch row stays visually fixed while the Settings list scrolls. A vertical drag starting on the row, label, or switch scrolls the list and never toggles the value. A tap on the row or switch toggles exactly once. Ordinary action rows keep their normal activation and scrolling behavior.
