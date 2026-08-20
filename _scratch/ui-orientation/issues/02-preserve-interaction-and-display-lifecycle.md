# 02 — Preserve interaction and display lifecycle during rotation

**What to build:** Let users reposition the device during normal use without interrupting an active touch, navigation state, display sleep behavior, or an active Pomodoro.

**Blocked by:** 01 — Rotate the complete UI from stable placement.

**Status:** done

- [x] A stable direction reached during an active touch or swipe remains pending until the touch is released.
- [x] Applying a pending direction after release does not activate, cancel, or redirect the interaction that just ended.
- [x] Rotation preserves the current page, Settings scroll position, open Dialog, overlays, Timer state, and absolute deadline.
- [x] Main, Menu, Statistics, Settings, Numeric Stepper, Dialogs, overlays, buttons, and edge gestures remain correctly aligned with touch in every UI Orientation.
- [x] Direction sampling continues while the display is off, and wake displays the latest stable direction.
- [x] The first touch while the display is off continues to wake only the display and never executes the underlying control.
- [x] Existing dimming, sleep, pixel-shift, completion-wake, and timer-accuracy behavior remains unchanged.
- [x] Host tests verify active-touch deferral and observable state preservation without asserting LVGL internals.
- [x] The complete host test suite and ESP-IDF firmware build pass.
