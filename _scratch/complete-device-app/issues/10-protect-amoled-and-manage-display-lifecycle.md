# 10 — Protect the AMOLED and manage display lifecycle

**What to build:** Apply the specified idle, wake, and pixel-protection behavior so the AMOLED remains usable and protected without making a Running timer disappear or allowing a wake touch to trigger an action accidentally.

**Blocked by:** 03 — Complete the Main timer and Pomodoro Set; 07 — Complete Settings and Numeric Stepper; 09 — Connect sound, buttons, battery, and power.

**Status:** ready-for-human

- [x] Running dims to 60% of chosen brightness after two idle minutes but remains visible.
- [x] Ready, Paused, and waiting Completion Transition dim to 20% after two minutes and turn the display off after five minutes.
- [x] The first touch while off wakes only the display; later input works normally and restores chosen brightness.
- [x] Timer completion wakes the display and preserves visual feedback even when sound is muted.
- [x] Static content receives an unobtrusive periodic ±2 px shift without moving blocking dialogs incorrectly.
- [x] Host timing tests and hardware tests cover idle thresholds, wake consumption, completion wake, and brightness restoration.

## Comments

2026-08-16: Display policy is extracted as a pure application service and host-tested for dim, sleep, wake consumption, completion wake, brightness restoration, and pixel shift. SH8601/AXP2101 device integration builds successfully; on-device lifecycle checks remain pending in `docs/HARDWARE_CONNECTION_TEST.md`.
