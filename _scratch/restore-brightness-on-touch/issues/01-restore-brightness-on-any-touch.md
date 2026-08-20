# 01 — Restore brightness on any touch

**What to build:** Make every effective screen touch restore the user's chosen brightness and reset idle timing. A dimmed display must continue the current button or gesture action, while the first touch after the display is off must wake only and be consumed. Include host coverage, firmware build verification, and the corresponding product and hardware documentation updates.

**Blocked by:** None — can start immediately.

**Status:** ready-for-human

- [x] Touching blank space, a button, or the start of a swipe restores the chosen brightness.
- [x] A touch on a dimmed but awake display restores brightness and still performs the intended action.
- [x] Touch interaction restarts the idle timing window.
- [x] The first touch while the display is off restores brightness without triggering a click, gesture, or value change.
- [x] The second touch after wake is dispatched normally.
- [x] Existing Pomodoro, Pomodoro Set, statistics, persistence, and completion-wake behavior remain unchanged.
- [x] Host tests and the ESP-IDF firmware build pass.
- [x] Product behavior and hardware acceptance documentation describe the updated touch behavior.

## Comments

- 2026-08-19: Implemented on `feat/touch-lighten-screen`. Touch handling is bound at the LVGL pointer indev PRESSED seam after the touch device is created and the application has booted, so blank-space, button, and swipe-start contacts all restore brightness and reset idle timing. A dimmed-but-awake touch continues to the pressed control; the first touch while off is consumed for the whole contact via `lv_indev_wait_release`.
- 2026-08-19: Review fixes applied. Pointer contacts are recorded exactly once (the action dispatch layer no longer re-records them; hardware buttons and TICK do not reset the idle window). A wake touch stays consumed even if brightness recovery fails. Idle reset boundaries are asserted at 119/120 and 299/300 seconds.
- 2026-08-19: Verification — host tests 3/3 pass (`cmake -S test -B /tmp/pomodoro-host-test && cmake --build /tmp/pomodoro-host-test && ctest --test-dir /tmp/pomodoro-host-test`); ESP-IDF build passes (`idf.py build`, `pomodoro.bin` 0x156540 bytes, 55% app partition free).
