# Adaptive UI Orientation

Status: done

## Problem Statement

The device currently assumes one fixed physical direction. When it is placed with another edge upward, the interface is sideways or upside down and touch coordinates no longer match the user's reading direction. Users need the complete interface to remain readable and touchable without manually changing a setting or disturbing an active Pomodoro.

## Solution

Use the target board's QMI8658 accelerometer to determine one of four stable UI Orientations and rotate the complete LVGL display and touch coordinate space together. Direction changes are automatic, debounced, deferred while a touch is active, and independent of Pomodoro state. Ambiguous placement or sensor failure preserves a usable last-known or default direction instead of blocking the timer.

## User Stories

1. As a device user, I want the interface to face me when any physical edge is uppermost, so that I can place the device where it fits on my desk.
2. As a device user, I want buttons and gestures to rotate with their visuals, so that touching what I see performs the expected action.
3. As a device user, I want the Main screen to adapt automatically, so that the countdown remains readable without manual configuration.
4. As a device user, I want Menu, Statistics, Settings, Numeric Stepper, dialogs, and overlays to use the same UI Orientation, so that navigation stays consistent.
5. As a device user, I want the current page to remain open during rotation, so that changing placement does not interrupt my task.
6. As a device user, I want scroll position to survive rotation, so that I do not lose my place in Settings.
7. As a device user, I want an open confirmation dialog to survive rotation, so that the pending decision is not accepted or cancelled accidentally.
8. As a device user, I want an active Pomodoro to keep the same deadline and state during rotation, so that physical movement cannot alter my focus record.
9. As a device user, I want direction changes to wait until the device is stable, so that normal handling does not make the UI jump repeatedly.
10. As a device user, I want diagonal, flat, or ambiguous placement to retain the current direction, so that the UI does not oscillate between orientations.
11. As a device user, I want a touch or swipe already in progress to finish before rotation, so that movement cannot redirect my input.
12. As a device user, I want rotation to happen without a heavy animation, so that touch response and timer accuracy remain responsive on constrained hardware.
13. As a device user, I want the device to boot immediately, so that orientation detection does not add a blank startup delay.
14. As a device user, I want the display to wake in the latest stable direction, so that I do not see a stale orientation after sleep.
15. As a device user, I want the timer to remain usable if the accelerometer fails, so that an auxiliary feature cannot block the primary product.
16. As a device user, I want one brief failure message, so that unavailable auto-rotation is understandable without repeated interruption.
17. As a device user, I want auto-rotation to resume after the sensor recovers, so that I do not need to restart the device.
18. As a device user, I want existing display dimming, sleep, wake-touch consumption, pixel shifting, and completion feedback to work in every direction, so that orientation support does not weaken existing behavior.
19. As a maintainer, I want UI Orientation excluded from persisted Pomodoro state, so that physical placement cannot cause schema changes or recovery ambiguity.
20. As a maintainer, I want accelerometer-only sensing, so that unused gyroscope and gesture behavior do not add power use or code.
21. As a maintainer, I want deterministic direction rules, so that boundary, debounce, and failure behavior can be verified without target hardware.
22. As a maintainer, I want target-board checks for rendering, touch, memory, and latency, so that software rotation is proven under the board's SRAM constraint.

## Implementation Decisions

- UI Orientation has four values: `0°`, `90°`, `180°`, and `270°`. The current fixed display direction is `0°`; the QMI8658 axes must be calibrated against that direction on the target board.
- Initialize only the QMI8658 accelerometer. Keep the gyroscope and IMU gesture features disabled.
- Add one small platform orientation resolver as the sole deterministic seam for sample classification, stability, active-touch deferral, and read-failure recovery. It accepts observations and emits only externally meaningful orientation or warning changes.
- Keep UI Orientation entirely within the board/display platform. Do not add it to the Pomodoro state machine, application view model, persisted state, or NVS schema.
- Reuse the existing periodic platform controls loop and sample at approximately 10Hz; do not add a dedicated task, event bus, or general-purpose sensor abstraction.
- A sample can nominate a direction only when the dominant screen-plane gravity component is at least `0.70g` and exceeds the other screen-plane axis by at least `0.15g`.
- Require six consecutive valid samples for the same candidate direction. An invalid, flat, or diagonal sample clears the candidate count while preserving the current UI Orientation.
- If a stable direction is reached during an active touch, retain it as pending and apply it after release. Never rotate the display in the middle of a touch or swipe.
- Continue sampling while the display is asleep. Waking displays the latest stable UI Orientation, while the first wake touch retains its existing wake-only behavior.
- Use LVGL display rotation so LVGL rotates the touch coordinate space with the rendered output. Enable the existing `esp_lvgl_port` software-rotation path because the target SH8601 driver cannot perform all four required hardware transforms.
- Keep the existing partial-draw-buffer strategy. Software rotation may allocate one additional rotation buffer of approximately 19.2KB; do not introduce a full framebuffer or double buffering.
- Apply a stable direction as an immediate full redraw without rotation animation. Preserve the active page, scroll position, dialog, overlays, Timer state, and deadline.
- Boot immediately in `0°`; do not wait for the initial six-sample classification before showing the interface.
- After three consecutive accelerometer read failures, preserve the last UI Orientation, mark auto-rotation unavailable, and retry sensor initialization every five seconds.
- Show `Auto-rotate unavailable` once per boot as a non-blocking three-second status message. Sensor recovery is silent and resumes normal classification.
- Auto-rotation is always enabled. Do not add a setting, direction lock, calibration UI, or persisted preference.
- Update the existing product constraint so that the accelerometer and four-way UI Orientation are in scope while gyroscope-driven behavior and IMU gestures remain out of scope.

## Testing Decisions

- Good automated tests drive the platform orientation resolver with acceleration, touch-state, and read-result observations, then assert only emitted UI Orientation and warning behavior. They do not assert register transactions, counters, private fields, or LVGL internals.
- Host-test all four direction classifications using the target-board axis mapping once calibrated.
- Host-test the `0.70g` validity threshold, `0.15g` dominance margin, six-sample stability requirement, candidate reset on ambiguous samples, and preservation of the current direction.
- Host-test that a stable change is deferred during an active touch and emitted after release.
- Host-test that three consecutive read failures trigger one unavailable warning, preserve orientation, and enter recovery; successful recovery resumes classification without a success message.
- Follow the repository's existing assert-based C host-test style. Keep the resolver free of ESP-IDF and LVGL dependencies so the existing host test target can exercise it.
- Keep the existing application orchestrator tests unchanged except where necessary to prove Timer state, page state, persistence, and deadlines remain unaffected by platform-only rotation.
- On the target board, verify Main, Menu, Statistics, Settings, Numeric Stepper, dialogs, overlays, edge gestures, and ordinary touch targets in all four UI Orientations.
- On the target board, verify that rotation waits for touch release, flat and diagonal placement do not oscillate, sleep/wake uses the latest direction, and sensor failure leaves the timer usable.
- Measure free memory, frame time, and touch latency with software rotation enabled. Confirm that no full framebuffer or double buffer is allocated and that timer accuracy remains within its existing requirement.
- Run the complete host test suite and the ESP-IDF firmware build after integration.

## Out of Scope

- Manual direction selection, orientation lock, an auto-rotation setting, or persistence of UI Orientation.
- Layout reflow or separate portrait/landscape designs; the physical display remains a square 480×480 layout.
- Rotation animation or follow-motion rendering.
- Gyroscope use, motion gestures, step counting, tilt-angle UI, sensor fusion, or a reusable IMU framework.
- User calibration controls. Initial sensor-to-screen axis calibration is a target-board implementation task.
- Changes to Pomodoro rules, Timer persistence, statistics, navigation semantics, or display lifecycle policy.
- Replacing the SH8601 driver or implementing unsupported hardware rotation commands.

## Further Notes

- The target has 512KB HP SRAM and no PSRAM, so target-board memory and responsiveness checks are required before marking the feature complete.
- The existing product and prototype documents already define the UI Orientation behavior and the `Auto-rotate unavailable` status presentation.
- No ADR is required: the feature is isolated to the platform boundary, uses existing LVGL capabilities, and remains straightforward to revise if a future display driver adds complete hardware rotation.
