# 01 — Add device OpenCode Status support

**What to build:** Implement the documented `OC1` USB status protocol, persisted default-on `OpenCode status` setting, Main bottom-left status circle, acknowledgement behavior, display wake policy, connection timeout, and host-testable state handling without changing Pomodoro behavior.

**Blocked by:** None — can start immediately from the accepted spec and ADR 0010.

**Status:** done

- [x] The device accepts only length-bounded, valid `OC1 STATE` lines and ignores malformed, unknown-version, or stale input without affecting the timer.
- [x] Aggregate priority is Error, Waiting, Working, Completed, Idle; repeated heartbeats with the same revision do not wake or redraw unnecessarily.
- [x] Error remains red until acknowledged, new work, or disconnect; Completed remains green for five seconds or until acknowledged; acknowledgement reveals the next active priority.
- [x] Main shows the 28 px circle at the documented bottom-left position, vertically center-aligned with the Sound button, with a 44 px touch target, correct palette colors, pixel shift, UI Orientation behavior, and blocking-dialog protection.
- [x] Valid external status changes wake both automatically and manually slept displays; heartbeat, acknowledgement, expiry, and timeout do not repeat the wake.
- [x] Settings contains a persisted, default-on `OpenCode status` switch.
- [x] Off hides the indicator, stops OpenCode status reads and wakeups, and emits `DISABLED`; On emits `ENABLED` and resumes synchronization.
- [x] Ten seconds without a valid enabled heartbeat hides the indicator; reconnect restores the current status.
- [x] Native tests cover parsing, state priority, revision handling, acknowledgement, expiry, timeout, wake policy, and switch persistence without adding an LVGL test framework.

## Comments

- 2026-08-21: Existing persisted-data compatibility was explicitly waived; no OpenCode status migration is required.
