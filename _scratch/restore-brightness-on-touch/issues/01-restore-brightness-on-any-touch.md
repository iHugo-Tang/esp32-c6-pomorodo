# 01 — Restore brightness on any touch

**What to build:** Make every effective screen touch restore the user's chosen brightness and reset idle timing. A dimmed display must continue the current button or gesture action, while the first touch after the display is off must wake only and be consumed. Include host coverage, firmware build verification, and the corresponding product and hardware documentation updates.

**Blocked by:** None — can start immediately.

**Status:** done

- [x] Touching blank space, a button, or the start of a swipe restores the chosen brightness.
- [x] A touch on a dimmed but awake display restores brightness and still performs the intended action.
- [x] Touch interaction restarts the idle timing window.
- [x] The first touch while the display is off restores brightness without triggering a click, gesture, or value change.
- [x] The second touch after wake is dispatched normally.
- [x] Existing Pomodoro, Pomodoro Set, statistics, persistence, and completion-wake behavior remain unchanged.
- [x] Host tests and the ESP-IDF firmware build pass.
- [x] Product behavior and hardware acceptance documentation describe the updated touch behavior.
