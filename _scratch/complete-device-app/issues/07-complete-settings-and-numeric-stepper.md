# 07 — Complete Settings and Numeric Stepper

**What to build:** Deliver the full Settings experience so the user can inspect and change timer, automation, sound, display, date/time, statistics, and power preferences with the specified save and cancellation behavior.

**Blocked by:** 03 — Complete the Main timer and Pomodoro Set; 04 — Recover a timer without data loss; 05 — Set local time and handle RTC failure; 06 — Show Today and the Trailing Seven Days.

**Status:** ready-for-human

- [x] Settings renders the specified section order, scrolls below a fixed app bar, and exposes Today's statistics.
- [x] Focus, Short break, Long break, and Daily Goal steppers enforce their ranges and save only on Confirm; Back discards drafts.
- [x] Duration changes never alter a Running or Paused timer and apply the next time that Mode starts.
- [x] Auto-start and Sound switches save immediately and remain synchronized with Main shortcuts and the stored volume state.
- [x] Brightness previews from 10%–100% in 5% steps and persists on release.
- [x] Date/time and power rows show current resolved values, and all confirmed changes survive reboot.

## Comments

2026-08-16: Settings, shared NumericStepper behavior, immediate and confirmed persistence rules, brightness preview/release, fixed app bars, and child-page edge navigation are implemented. Host tests cover persistence, cancellation, active-timer isolation, and reboot restoration.
