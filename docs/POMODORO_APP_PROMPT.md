# Pomodoro App Structure Prompt

Design and implement a single-purpose Pomodoro timer for the Waveshare ESP32-C6-Touch-AMOLED-2.16. The product runs locally on a 480×480 touch AMOLED display. It has no account, cloud service, notification shade, background internet access, or general-purpose app features. All user-facing copy is English.

## Target hardware and framework

- Board: Waveshare ESP32-C6-Touch-AMOLED-2.16.
- Display: 480×480 AMOLED using SH8601 over QSPI.
- Touch: CST9217 capacitive touch.
- Clock: battery-backed PCF85063 RTC.
- Audio: ES8311 codec and onboard speaker path.
- Power: AXP2101 PMIC and optional lithium battery.
- Storage: 16MB internal Flash; do not depend on MicroSD.
- Memory constraint: 512KB HP SRAM and no PSRAM.
- Framework: ESP-IDF `>=5.5.2,<5.6` with pinned, Waveshare-validated LVGL 9, `esp_lvgl_port`, and Waveshare driver versions.
- Use small partial LVGL draw buffers. Never allocate a full RGB565 framebuffer or full-screen double buffers.
- Bluetooth, Thread, Zigbee, microphones, ES7210, IMU, and MicroSD remain uninitialized. Wi-Fi is enabled only for an explicit iPhone time-sync session.

## Product language

Use these canonical English terms throughout UI, design, code, and tests:

- Modes: `Focus`, `Short break`, `Long break`.
- Actions: `Start`, `Pause`, `Resume`, `Reset`, `Confirm`.
- States: `Ready to focus`, `Ready for a break`, `Focusing`, `Taking a short break`, `Taking a long break`, `Paused`.
- Completion: `Focus complete` / `Great work! +1`; `Break over` / `Ready for another focus?`.
- Progress: `Set 3/4`.
- Statistics: `Daily goal`, `Focus time`, `Goal`, `Statistics`.
- Use sentence case, not all caps.
- Do not use system Emoji as required UI. Render the tomato as an app-owned icon or illustration.

## Modes and duration settings

The main screen has three fixed, non-wrapping pages in this order:

1. Focus — red accent — default 25 minutes — adjustable 5–60 minutes.
2. Short break — green accent — default 5 minutes — adjustable 1–30 minutes.
3. Long break — blue accent — default 15 minutes — adjustable 5–60 minutes.

All duration steppers use one-minute increments. A duration change never alters a running or paused timer; it applies the next time that mode starts.

## Main screen

The main timer screen is centered, has no scrolling, and respects a minimum 24px safe margin.

Top app bar:

- Left: current mode icon and mode name.
- Right: battery/power state, Sound shortcut, Statistics icon, Settings icon.
- The Sound shortcut and Settings Sound switch control the same state.

Center:

- A CircularTimer containing a monospaced, tabular `MM:SS` countdown and the primary action button; do not show a state-description label inside the ring.
- Only the explicit button triggers Start/Pause/Resume; tapping the ring or digits does nothing.
- When paused, hide the countdown and show compact `Resume` plus a lower-emphasis `Reset` action as one vertically centered group inside the ring.
- Reset requires a confirmation dialog and creates an Aborted Timer.
- The colored ring represents remaining time. It starts full and disappears clockwise from 12 o’clock, revealing a muted track.
- The ring updates once per second. Paused state stops progress and may use a low-frequency, low-amplitude breathing effect.

Bottom:

- First row: `Set N/4` plus four tomato outline markers. Completed markers are filled with the Focus accent.
- Second row: three non-interactive mode pagination dots.
- Pagination dots are display-only and cannot be tapped.

## Timer state machine

Each timer has these states:

- Ready: show configured duration and `Start`, without a state-description label.
- Running: calculate remaining time from an absolute deadline; show `Pause`, without a state-description label.
- Paused: preserve but hide remaining time; show compact `Resume` and `Reset` controls inside the ring.
- Completion transition: replace the central timer content with the completion illustration, copy, and actions. Do not use a floating Dialog for normal completion.

Switching modes:

- Swipe left for the next mode and right for the previous mode.
- Do not wrap from Focus to Long break or vice versa; show a small boundary response.
- An idle timer switches immediately.
- A running or paused timer requires `End this timer and switch modes?` confirmation.
- Confirming discards progress, does not add a Pomodoro, and does not advance Set Progress.
- A completion transition blocks mode switching and navigation until it resolves.

## Pomodoro Set rules

- A Pomodoro is counted only when a Focus timer naturally completes.
- Aborted or reset Focus timers never count.
- A Pomodoro Set contains four completed Pomodoros.
- After Pomodoros 1–3, the scheduled next mode is Short break.
- After Pomodoro 4, the scheduled next mode is Long break.
- `4/4` remains visible through the Focus completion state and the following Long break.
- Completing any Long break resets Set Progress to `0/4`, including a manually selected early Long break.
- If the user reaches `4/4`, skips Long break, and starts another Focus, reset to `0/4` when that Focus starts; it becomes `1/4` when completed.
- Midnight never resets Set Progress. A set may span Statistics Days.
- Reaching the Daily Goal never stops or resets the set.

## Completion and automation

Settings defaults:

- Auto-start breaks: on.
- Auto-start focus: off.

When the relevant auto-start setting is on:

- Show the completion state for three seconds.
- Show a visible countdown to the scheduled next mode.
- Offer `Start now` and `Cancel auto-start`.
- Cancel affects this transition only and does not change the global setting.
- Do not run additional chained modes while the device is powered off.

When auto-start is off:

- Keep the completion state visible until the user acts.
- Focus completion offers only `Start break`; the application chooses Short break or Long break from Set progress.
- Break completion offers only `Start focus`.

Completion visuals:

- Focus: a small smiling tomato illustration, limited confetti, `Focus complete`, `Great work! +1`.
- Break: a small steaming cup illustration, `Break over`, `Ready for another focus?`.
- On the first Pomodoro that reaches the Daily Goal, add `Daily goal reached!` and slightly richer confetti. Do not create a second modal or repeat the extra celebration for later Pomodoros.
- Completion graphics are flat, rounded, app-owned vector/bitmap assets on transparent backgrounds. Avoid photos, 3D rendering, brands, and Emoji.

## Navigation and Android-style behavior

- Main top-bar Statistics icon opens Stats.
- Main top-bar Settings icon opens Settings.
- Settings retains a `Today’s statistics` row.
- Child pages use a top back arrow and a left-edge swipe-back gesture.
- Stats returns to its source: Main if opened from Main, Settings if opened from Settings.
- Reserve the left 32px edge for Back on child pages. A committed edge gesture requires roughly 72px horizontal movement and clear horizontal dominance; tune on hardware.
- Do not use custom top-down or bottom-up gestures.
- Main mode swipes start outside the reserved edge area and execute only one action per gesture.
- A running timer continues behind Settings and Stats. If it completes, play feedback and return to Main completion state. A paused timer remains paused.

## Settings

Settings uses a fixed top app bar and an Android-style vertically scrolling list. Changes save immediately except steppers and date/time editors, which require Confirm.

Sections, in order:

1. Statistics
   - Today’s statistics
2. Timer
   - Focus duration
   - Short break duration
   - Long break duration
   - Daily goal
3. Automation
   - Auto-start breaks
   - Auto-start focus
4. Sound
   - Sound switch
   - Current volume, for example `3/5`
   - `Use the side buttons to adjust volume`
5. Display
   - Brightness
6. Date & time
   - Current local date and time
   - Time zone
   - Sync from iPhone
   - Set manually
7. Power
   - Battery percentage and charging state

There is no Vibration setting and no haptic behavior. There is no clear-statistics action.

NumericStepper behavior:

- Use up/down arrows and a wheel-style numeric selector.
- Confirm saves and returns to Settings.
- Back discards unconfirmed changes without another dialog.

Daily Goal:

- Range 1–16, step 1, default 8.
- Changing it immediately recalculates today’s percentage without altering completed data.

Brightness:

- Range 10%–100%, step 5%, default 70%.
- Preview while dragging; save on release.

## Statistics

Stats is a compact, non-scrolling single screen:

- Fixed top bar: Back and `Statistics`.
- Primary metric: `Today · × N`.
- Two side-by-side Cards: `Focus time` and `Goal`.
- Bottom chart: completed Pomodoros for the trailing seven Statistics Days, including today.

Statistics rules:

- A Statistics Day is local 00:00 to the next local 00:00.
- A Focus spanning midnight is attributed wholly to the day on which it completes.
- Completed Focus Duration is the sum of configured durations of naturally completed Focus timers. Paused time, aborted timers, and the current incomplete timer do not count.
- Goal percentage may exceed 100%; for example 10/8 displays 125%. Visual fill caps at 100%.
- The chart runs oldest-to-newest with today at the right.
- Use `M/D` date labels, highlight today, and show counts above bars.
- Bar height represents completed Pomodoros, not minutes.
- Scale to the trailing-seven-day maximum while covering at least the current Daily Goal.
- Persist 365 daily aggregates. Display only the latest seven. Delete the oldest when day 366 is created.

## Date, time, and iPhone sync

The RTC stores UTC. Local time and Statistics Day use the saved time-zone offset schedule.

First boot is not skippable:

- Offer `Sync from iPhone` and `Set manually`.
- Other settings use their defaults.

iPhone sync:

- Start a temporary Wi-Fi SoftAP with a random password and a five-minute expiry.
- Display a QR code for the iPhone to join.
- Serve a local page that reads the iPhone’s calibrated UTC time, IANA time-zone name, and the next five years of UTC-offset transitions.
- Require `Sync to Pomodoro` confirmation before writing the RTC.
- Shut down Wi-Fi and delete all session data after success or expiry.
- Never connect to the home router, access the internet, or store Wi-Fi credentials.
- Prompt for another sync before the five-year transition schedule expires.

Manual time setup stores a fixed UTC offset. Automatic DST requires iPhone sync.

Changing date/time:

- Preserve an active timer’s remaining duration and recompute its deadline from the corrected clock.
- Do not migrate or relabel existing historical records.
- Future completions use the corrected current date; merge into an existing record for that date if present.

RTC failure:

- If invalid at boot, require sync or manual setup before Main.
- If RTC fails during a timer, continue using a monotonic clock.
- Hold completed results as Pending Statistics.
- Once time is restored, assign Pending Statistics to the restored current Statistics Day.
- Hide the trailing chart while time is invalid and show `Set date & time to view statistics`.

## Restart and persistence

- Persist current mode, timer state, configured duration snapshot, absolute deadline, settlement marker, and paused remaining time.
- Before the deadline, reboot restores the running timer from the RTC.
- After the deadline, reboot settles the current timer exactly once as naturally complete and displays completion state.
- Do not auto-run subsequent modes during power-off time.
- A paused timer restarts paused with unchanged remaining time.
- Save Settings, timer snapshots, and the 365-day ring in internal Flash/NVS with data versions and integrity checks.
- Write only on state transitions and confirmed setting changes, never once per second.

## Physical controls, sound, battery, and power

- `+/KEY`: increase prompt volume.
- `BOOT/-`: decrease prompt volume during normal runtime; preserve its bootloader/download behavior during startup.
- Volume range 0–5, default 3. Persist the last volume.
- Volume 0 is muted and synchronizes the Sound icon/switch.
- Sound toggling restores the last non-zero volume.
- Show a temporary on-screen volume overlay after a side-button change.
- `PWR`: short press sleeps/wakes the display; long press powers off according to board behavior.

Transient status overlay implementation:

- Main owns one screen-level `StatusOverlay` container for all non-blocking status feedback. RTC/date-time errors, the one-shot low-battery warning, and the side-button volume indicator must be children of this same container; do not create separate independently positioned overlay layers for them.
- Keep `StatusOverlay` above normal Main content and below blocking dialogs. It must not intercept touch input or pause the timer.
- Configure the container with LVGL flex layout using `LV_FLEX_FLOW_COLUMN`, centered on the horizontal axis below the top app bar. Use an 8px gap between visible children.
- Child order is fixed from top to bottom: `Date & time unavailable`, `Battery low`, then the volume indicator. Hidden children consume no layout space, so any visible combination remains vertically packed.
- The volume indicator remains a horizontal row inside its child surface: speaker/mute icon, five-segment level bar, and the numeric value such as `3/5`.
- Status messages retain their own visibility lifetimes. Updating or dismissing one child must not recreate, reposition independently, or dismiss the other visible children.

```text
StatusOverlay (single overlay parent, vertical flex)
├── RTC status badge
├── Battery-low badge
└── Volume surface (horizontal flex)
    ├── Speaker/mute icon
    ├── Five-segment level bar
    └── Numeric level
```

Sound occurs only on timer completion:

- Focus complete: two short rising tones.
- Break complete: one gentle tone.
- No click, navigation, reset, or warning sounds.
- Sound off leaves visual feedback intact.

Battery UI:

- Top bar uses a compact battery icon, charging bolt, or USB power icon.
- Below approximately 15%, use a warning color and show `Battery low` once.
- Low battery does not cancel the current timer.
- Settings shows the board’s approximate battery percentage and charging state.

## Display and AMOLED protection

- Running timer: after two minutes without touch, dim to 60% of the chosen brightness but remain visible.
- Ready, paused, or waiting completion state: after two minutes dim to 20% of chosen brightness; after five minutes turn the display off.
- First touch while off wakes the display only and does not execute the underlying action.
- Timer completion wakes the display and plays sound if enabled.
- Apply an unobtrusive ±2px periodic pixel shift to static content.
- User interaction immediately restores chosen brightness.

## Visual system

Fixed dark theme; no theme switcher.

Tokens:

- `--bg`: `#0F1115`
- `--surface`: `#1A1F27`
- `--surface-raised`: `#232A35`
- `--text-primary`: `#F5F7FA`
- `--text-secondary`: `#A7B0BE`
- `--accent-focus`: `#FF5C5C`
- `--accent-short-break`: `#4DD17A`
- `--accent-long-break`: `#5B8CFF`
- `--divider`: `#303846`

Typography:

- Inter for all interface text.
- Space Mono for `MM:SS`.
- Countdown around 64px; body at least 16px; supporting text at least 14px, adjusted only after physical-size testing.
- Every interactive target is at least 48×48px.
- Never communicate mode by color alone; always pair accent color with icon and text.

Components:

- Button
- IconButton
- Switch
- StatusBadge
- Card
- CircularTimer
- SetProgress (replaces ambiguous SessionDots terminology)
- ModePageIndicator
- NumericStepper
- Dialog for destructive confirmation only
- VolumeOverlay
- BatteryIndicator

## Motion and performance

- Touch and timer correctness always outrank animation.
- Mode swipes use a threshold gesture, then an approximately 120ms fade; no full-screen follow-finger rendering.
- Page transitions use a simple short fade, not full-page sliding.
- Digits and ring update once per second.
- Confetti uses a few prebuilt shapes and ends after roughly one second on this hardware.
- If frame time degrades, remove optional animation before changing interaction behavior.

## Accuracy and update requirements

- Remaining time is derived from an absolute deadline, never from decrementing a counter.
- UI time error must remain within ±1 second.
- Completion must be emitted within one second of the RTC deadline.
- Navigation, rendering, audio, and Flash writes must not introduce drift.
- Recompute immediately after waking the display.

Firmware updates:

- USB-C with ESP-IDF tools only; no OTA.
- Keep application and persistent-data partitions separate.
- Normal updates preserve Settings, active timer snapshot, and statistics.
- Version every persistent schema and migrate supported prior versions.
- Only an explicit full-chip erase removes user data.

## Out of scope

- Accounts, login, cloud sync, analytics, remote notifications, notification shade.
- Internet access, background Wi-Fi, saved Wi-Fi credentials, OTA.
- Vibration or haptics.
- Microphone, voice assistant, IMU gestures, screen rotation, Bluetooth, Thread/Zigbee, MicroSD features.
- Long-term statistics UI beyond the trailing seven-day chart.
- Clearing statistics from Settings.
