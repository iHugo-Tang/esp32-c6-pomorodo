# Display and input

## Battery-powered display lifecycle

After two minutes without screen interaction, a Running Timer dims to 60% of the chosen brightness but remains visible. Ready, Paused, and Completion Transition dim to the chosen idle brightness, which defaults to 20%. Those non-Running states turn the display off after five minutes.

Timer completion wakes the display even when sound is muted. Static content shifts unobtrusively by at most 2 px each minute; blocking dialogs do not move.

## USB Power

USB Power means external power is arriving through USB. It is independent of battery charging and USB serial state.

While USB Power is present, automatic dimming and automatic Display Off are disabled on every page and Timer state. The display uses the normal or preview brightness. A user-requested Display Off still wins, and pixel shift continues.

Connecting USB Power restores a display dimmed or turned off automatically, but does not wake one the user turned off. Removing USB Power restarts idle timing from that moment. A charging indication without USB Power does not disable the battery policy. USB transitions do not act as touch or change Timer state, Set Progress, Statistics, or persistence.

## Touch and wake

Every screen contact—blank space, button, or swipe start—restores the chosen brightness and restarts idle timing. If the display is awake but dim, that same contact performs its intended action exactly once.

If the display is off, the first contact wakes it and is consumed through release; it cannot click, swipe, or change a value. A later contact works normally. Physical buttons and Timer ticks are not screen contacts.

## PWR button

A short PWR press turns an awake display off or wakes an off display. One press is enough after either automatic or manual Display Off. Wake restores the chosen brightness and restarts idle timing.

Temporary UI-lock contention may delay the action but cannot lose it, and one physical press causes at most one display transition. Long press keeps the board's power-off behavior. Neither action changes the Timer deadline or Pomodoro state.

## Hardware status

Main shows battery percentage plus charging and USB Power state without interrupting a Running Timer. Low battery produces one non-blocking warning.

RTC warning, Battery low, and Volume feedback share one non-intercepting status overlay in that fixed order. The overlay never takes input or changes navigation, Timer state, Set Progress, or Statistics.

## UI Orientation

UI Orientation has four values: `0°`, `90°`, `180°`, and `270°`. The QMI8658 accelerometer selects the stable direction automatically; the gyroscope and IMU gestures remain disabled. Orientation is platform-only and is not stored in Pomodoro state, the application view model, NVS, or persistence.

The platform samples at about 10 Hz. A sample can nominate a direction only when the dominant screen-plane gravity component is at least `0.70g` and exceeds the other screen-plane component by at least `0.15g`. Six consecutive valid samples for one direction apply it. Flat, diagonal, or ambiguous samples clear the candidate and keep the current direction.

Boot shows immediately at `0°`. LVGL software rotation changes rendering and touch coordinates together, uses the existing partial-buffer design plus at most an approximately 19.2 KB rotation buffer, and never introduces a full RGB565 framebuffer, full-screen double buffering, or rotation animation.

A stable change during an active touch remains pending until release. Rotation preserves the current page, Settings scroll position, dialog, overlays, Timer state, and absolute deadline. Sampling continues while the display is off so wake uses the latest stable direction, while the first wake touch remains wake-only.

After three consecutive accelerometer read failures, keep the last orientation, stop classification, show `Auto-rotate unavailable` once per boot for three seconds without blocking input, and retry initialization every five seconds. Recovery is silent and resumes classification. Orientation failure must not block RTC, persistence, display, touch, controls, audio, or Pomodoro timing.

Software rotation must leave frame time and touch latency usable in every direction. Timer display error and completion timing remain within one second while orientation handling is active.

Settings-list gestures are specified in [Navigation and settings](navigation-and-settings.md#switch-rows).
