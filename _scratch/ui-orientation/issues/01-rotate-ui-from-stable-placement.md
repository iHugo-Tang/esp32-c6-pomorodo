# 01 — Rotate the complete UI from stable placement

**What to build:** Make the device detect a stable physical placement with the QMI8658 accelerometer and automatically rotate the complete interface and touch coordinates to the matching UI Orientation.

**Blocked by:** None — can start immediately.

**Status:** ready-for-agent

- [ ] Only the QMI8658 accelerometer is initialized; the gyroscope and gesture features remain disabled.
- [ ] The current fixed display direction is calibrated as `0°`, and all four UI Orientations map correctly on the target board.
- [ ] Acceleration is sampled at approximately 10Hz using the existing platform polling loop rather than a new task.
- [ ] A direction is valid only when its dominant screen-plane gravity component is at least `0.70g` and exceeds the other screen-plane axis by at least `0.15g`.
- [ ] Six consecutive valid samples for the same direction cause one rotation; an invalid, flat, or diagonal sample clears the candidate without changing the current direction.
- [ ] LVGL software rotation updates the complete display and touch coordinate space together without a full framebuffer, double buffering, or rotation animation.
- [ ] Boot displays immediately in `0°` and applies the first stable direction when available.
- [ ] UI Orientation remains platform-only and is absent from Pomodoro state, the application view model, NVS, and persisted schemas.
- [ ] Assert-based host tests cover all four directions, thresholds, stability, candidate reset, and unchanged-orientation cases through one deterministic orientation-resolver seam.
- [ ] The complete host test suite and ESP-IDF firmware build pass.
