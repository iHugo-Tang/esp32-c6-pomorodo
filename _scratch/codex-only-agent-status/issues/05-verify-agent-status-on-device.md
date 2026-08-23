# 05 — Verify Agent Status on the target device

**What to build:** Verify on the target board that the Codex-only Agent Status integration retains all visible and operational behavior after the domain, protocol, persistence, IPC, and state-directory changes.

**Blocked by:** 04 — Align current docs and verify the integration.

**Status:** ready-for-human

- [ ] Settings displays the `Agent` section and default-on `Agent status` switch.
- [x] Selecting a Codex Workspace displays its Idle, Working, Waiting, Completed, and Error states with the documented colors and priority.
- [ ] Acknowledging Error or Completed reveals the next state only within the current revision.
- [x] Completed returns to Idle after its display period, and a lost heartbeat hides the indicator after its timeout.
- [x] Meaningful Agent Status changes wake a dimmed or sleeping display without changing the current Pomodoro.
- [ ] Turning Agent Status off hides the indicator and prevents status-triggered wake; turning it on resumes synchronization.
- [ ] The Selected Agent Source remains a Codex Workspace and does not automatically fall back when it disappears.
- [x] Firmware flashing acquires the reservation in the new state directory, releases the serial port, completes, and allows Agent Status to reconnect.
- [x] Rebooting after the development persistence-version reset starts normally with defaults rather than showing a storage error.
- [ ] No regression is observed in Mode, Pomodoro completion, Set Progress, Statistics, display lifecycle, touch, sound, or UI Orientation.
- [x] The target-device acceptance result is recorded in the ticket comments or an attached artifact.

## Comments

- 2026-08-23: Flashed `/dev/cu.usbmodem11101` through `tools/flash.sh`; the reservation used the new Agent Status state directory, all image hashes verified, the device reset successfully, and an idle broker subsequently reconnected without error. Serial output after reboot repeatedly reported `OC1 ENABLED`, confirming the reset started with the default-on setting and no Storage Error. Direct OC1 state injection displayed Idle (`#454B56`), Working (`#5B8CFF`), Waiting (`#FFD166`), Completed (`#4DD17A`), and Error (`#FF5C5C`) on the target. Waiting/Error restored the main display without changing its 25:00 Focus state. Completed returned to Idle after five seconds; the indicator disappeared after the ten-second heartbeat timeout. Evidence is attached as `device-after-flash.png`, `device-{idle,working,waiting,completed,error}.png`, `device-completed-expired.png`, and `device-heartbeat-lost.png`. Settings navigation, touch ACK, switch interaction, and the broad physical control/sound/orientation regression remain for human operation.
