# 02 — Verify touch brightness recovery on the target device

**What to build:** Verify on the target board that touch-based brightness recovery works across ordinary touches, buttons, gestures, wake consumption, and all four UI Orientations, with no duplicate action or accidental wake action.

**Blocked by:** 01 — Restore brightness on any touch.

**Status:** ready-for-human

- [x] A blank-space touch after dimming restores the chosen brightness.
- [x] A button touch after dimming restores brightness and performs exactly one action.
- [x] A swipe starting after dimming restores brightness and completes the expected gesture.
- [x] The first touch after the display is off wakes only the display and does not activate the touched control.
- [x] A second touch after wake activates the intended control normally.
- [x] The behavior is correct in `0°`, `90°`, `180°`, and `270°` UI Orientation.
- [x] No timer, Pomodoro Set, statistics, or persistence regression is observed.
- [x] The target-board acceptance record is updated with the results.
