# 03 — Verify OpenCode Status end to end

**What to verify:** Validate the complete macOS OpenCode-to-device path on the target board, including colors, placement, touch acknowledgement, wake behavior, settings, disconnection, port contention, reconnection, rotation, AMOLED protection, and unchanged Pomodoro behavior.

**Blocked by:** 01 — Add device OpenCode Status support; 02 — Add the project OpenCode Status plugin.

**Status:** ready-for-human

- [x] Idle is gray, Working is `#5B8CFF`, Waiting is yellow, Completed is green, and Error is red; concurrent sessions follow the documented priority.
- [x] The circle is 28 px at the Main bottom-left, 26 px from the left edge and vertically center-aligned with the Sound button, has a reliable 44 px touch target, is absent outside Main, and cannot be activated through blocking dialogs.
- [x] Completed returns to Idle after five seconds and can be acknowledged early; Error persists until acknowledged, new work, or disconnect and then reveals the next state.
- [x] Every valid aggregate state change wakes displays turned off by timeout or PWR; repeated heartbeats, acknowledgement, completion expiry, and disconnect timeout do not cause repeated wakes.
- [x] The indicator follows all four UI Orientations and the existing AMOLED pixel shift without overlapping the CircularTimer, Set Progress, mode dots, or Sound button.
- [x] The default-on setting survives reboot; Off hides the circle, stops reads and wakeups, and On restores the current state without replaying stale messages.
- [x] USB unplug/replug and temporary ownership by flashing or monitoring tools hide after timeout and recover automatically without affecting OpenCode.
- [x] The full native test suite and firmware build pass, and Working, Waiting for question, Waiting for permission, Completed, Idle, Error, and concurrent-session priority are exercised end to end.
- [x] Timer accuracy, Pomodoro completion, Pomodoro Set, Settings, Statistics, touch gestures, sound, power controls, and normal USB firmware updates remain unchanged.
