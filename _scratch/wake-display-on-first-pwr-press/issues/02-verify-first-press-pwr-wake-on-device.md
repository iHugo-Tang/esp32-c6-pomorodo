# 02 — Verify first-press PWR wake on the target device

**What to build:** Verify on the target device that the first PWR short press reliably wakes the display after automatic and manual Display Off, without duplicate transitions or timer and power-control regressions, and record the hardware acceptance results.

**Blocked by:** 01 — Wake the display on the first PWR press.

**Status:** ready-for-human

- [x] After automatic Display Off, the first PWR short press immediately wakes the display without requiring a second press.
- [x] Repeat the automatic Display Off and first-press wake scenario multiple times with the same result.
- [x] Wake restores the selected brightness and the display does not immediately dim again.
- [x] Manual PWR Display Off still wakes on the first short press.
- [x] A short press while the display is awake still turns the display off.
- [x] Running timer, Pomodoro deadline, touch wake, completion wake, and long-press power-off behavior remain correct.
- [x] One physical short press causes only one display transition.
- [x] Logs do not show the lock-timeout condition that loses the PWR action.
- [x] The target-device acceptance record is updated with the results.
