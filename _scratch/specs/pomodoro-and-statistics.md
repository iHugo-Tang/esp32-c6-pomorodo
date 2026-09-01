# Pomodoro state and statistics

## Timer

The three Modes are Focus, Short break, and Long break. Each can be Ready, Running, Paused, or in Completion Transition. Starting is explicit unless auto-start is enabled. Main shows the remaining `MM:SS` and ring progress from an absolute deadline, updating once per second with no more than one second of visible timing error.

Changing Mode or resetting an unfinished Timer requires confirmation. Confirming creates an Aborted Timer: it does not count a Pomodoro or advance Set Progress. Completion Transition blocks navigation and Mode changes until the next step is started or cancelled. Focus transition duration is configurable from 1 to 60 seconds and defaults to 10 seconds; break transition duration is 3 seconds.

## Pomodoro Set

Only a naturally completed Focus counts as a Pomodoro. Pomodoros one through three lead to Short break; the fourth leads to Long break. Set Progress stays at `4/4` through the fourth completion and Long break, then resets when Long break completes. A manually selected Long break also ends the current Pomodoro Set when it completes.

Skipping Long break keeps `4/4` until the next Focus starts. A Pomodoro Set can cross a Statistics Day and never resets only because the date changed.

`Clear Set Progress` is available from Menu and requires confirmation. Cancel returns to Menu. Confirm resets progress to `0/4`, persists immediately, and returns to Main without changing the active Timer or Statistics. If confirmed during Completion Transition, the next Mode is Short break.

## Recovery

Persistent Timer state includes Mode, run state, and expected end time. A Running Timer resumes from its RTC deadline. If that deadline passed while power was off, it settles exactly once as a natural completion and does not auto-start another Mode. A Paused Timer restores its remaining duration and ignores powered-off time.

Persistence distinguishes unsettled from settled completion so reboot cannot count the same Pomodoro twice. Firmware-update and format compatibility rules live in [Firmware boundaries](firmware-boundaries.md#persistence-formats).

## Statistics

Statistics shows today's completed Pomodoros, Completed Focus Duration, Daily Goal progress, and the Trailing Seven Days. Daily Goal is independent of Pomodoro Set and does not stop or reset it when reached.

A naturally completed Focus contributes its configured duration to the Statistics Day in which it completes. An incomplete or Aborted Timer contributes nothing. Work is never split across midnight. RTC-invalid completions follow [Pending Statistics](date-and-time.md#local-wall-time).

The device retains the latest 365 Statistics Days. Each daily aggregate stores its local date, completed Pomodoro count, and Completed Focus Duration. Adding a 366th day removes the oldest aggregate; the seven-day UI does not reduce this retention requirement.
