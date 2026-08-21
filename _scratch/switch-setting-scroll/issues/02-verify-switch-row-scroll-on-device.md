# 02 — Verify switch-row scrolling on the target device

**What to build:** Verify on the target device in the default UI Orientation that dragging from each switch row scrolls the Settings list without moving the row's contents or accidentally changing its state, and that normal taps still toggle each switch exactly once.

**Blocked by:** 01 — Route switch-row drags to the Settings list.

**Status:** ready-for-human

- [x] Dragging from the row background, label, and switch scrolls the Settings list while the row contents remain fixed.
- [x] Auto-start breaks, Auto-start focus, and Sound do not change state during a drag.
- [x] Tapping either the row or its switch toggles the corresponding setting exactly once.
- [x] Settings rows without switches retain their existing scrolling and activation behavior.
- [x] The checks pass on the target device in the default UI Orientation.
