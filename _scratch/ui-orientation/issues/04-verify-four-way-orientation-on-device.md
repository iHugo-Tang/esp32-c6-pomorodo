# 04 — Verify four-way UI Orientation on the target device

**What to build:** Prove on the target board that automatic four-way UI Orientation is readable, touch-correct, responsive, memory-safe, and independent of Pomodoro timing across normal, sleeping, and degraded operation.

**Blocked by:** 02 — Preserve interaction and display lifecycle during rotation; 03 — Recover from orientation sensor failure.

**Status:** ready-for-agent

- [x] The hardware checklist records the calibrated QMI8658-to-screen mapping for `0°`, `90°`, `180°`, and `270°`.
- [ ] Main, Menu, Statistics, Settings, Numeric Stepper, Dialogs, overlays, ordinary controls, and edge gestures are exercised in all four directions with correct touch coordinates.
- [ ] Physical repositioning demonstrates the 600ms stability behavior, diagonal and flat-position retention, and touch-release deferral without visible oscillation.
- [ ] Rotation preserves page, scroll, Dialog, Timer state, deadline, and completion behavior during a running Pomodoro.
- [ ] Display sleep, direction changes while asleep, wake-only first touch, dimming, pixel shifting, and completion wake are verified together.
- [ ] Accelerometer failure shows one three-second non-blocking warning, leaves the timer usable, retries automatically, and resumes rotation after recovery.
- [ ] Free memory is recorded before and after software rotation setup, confirming no full framebuffer or double buffer and accounting for the additional partial rotation buffer.
- [ ] Frame time and touch latency remain usable in every direction, and timer display error and completion timing remain within the existing one-second requirement.
- [ ] The complete host test suite and ESP-IDF firmware build pass, and all target-board observations are recorded before the feature is marked resolved.
