# 01 — Wake the display on the first PWR press

**What to build:** Fix the lost PWR short press after automatic Display Off so one press wakes the display, restores the user's chosen brightness, and resets idle timing, while preserving timer continuity and existing manual display, touch-wake, and power-off behavior. Include the application regression coverage and firmware build verification.

**Blocked by:** None — can start immediately.

**Status:** ready-for-human

- [x] After automatic Display Off, the first PWR short press immediately wakes the display.
- [x] PWR wake restores the user's chosen brightness and resets the idle timing window.
- [x] A single physical PWR short press causes at most one display transition, including when hardware-action dispatch must be retried.
- [x] The display remains at normal brightness for the existing post-wake idle window and then follows the existing display policy.
- [x] Manual PWR Display Off and wake behavior remains unchanged.
- [x] Running timer, Pomodoro deadline, touch wake, completion wake, and long-press power-off behavior remain unchanged.
- [x] Host regression tests cover automatic Display Off followed by the first PWR short press and verify externally observable display and timer behavior.
- [x] The complete host test suite and ESP-IDF firmware build pass.
- [x] The target device confirms the fix without requiring a second press after automatic Display Off.
