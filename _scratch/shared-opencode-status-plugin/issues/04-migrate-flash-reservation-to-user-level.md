# 04 — User-level flash reservation

**What to build:** Moving the firmware flashing reservation from the project directory to the shared user-level state directory so that flashing works even when the selected OpenCode process belongs to another project: `tools/flash.sh` writes the reservation to the shared path before opening the port, waits a bounded time for the plugin to release the serial link, and always removes it on success, failure, interruption, or timeout without signaling OpenCode. The reservation check script follows the same path.

**Blocked by:** 02 — Add the global plugin wiring.

**Status:** ready-for-human

- [x] `tools/flash.sh` creates the reservation marker (containing its PID) in the shared user-level state directory used by all plugin instances, and waits for the serial port to be released before flashing.
- [x] The release wait is bounded by `POMODORO_FLASH_RELEASE_TIMEOUT_S` and fails with a clear message on timeout; the marker is removed on success, build/flash failure, timeout, and interruption.
- [x] The script never sends stop or termination signals to any OpenCode process.
- [x] With the selected OpenCode running in a different project, flashing from the Pomodoro repository still releases the serial link and the plugin reconnects automatically after the reservation is removed.
- [x] The reservation check script covers acquisition, stale-PID release, cleanup on success and failure, bounded-timeout failure, and live-owner protection, all against the shared path.

## Comments

- 2026-08-21: `tools/check-flash-reservation.sh` passes 10 checks, including live-owner non-clobber, stale-PID reclaim, replacement-owner preservation, success/failure/TERM/INT cleanup, bounded port-release timeout, and bounded non-zero failure under cleanup-lock contention.
- 2026-08-21: The user-config plugin check now covers the real wiring transition from active serial ownership through a live flash reservation and back to an automatic connection after release; `bun plugin/check-opencode-status.mjs` passes 125 checks. The unchecked cross-project, restart-based device acceptance still requires fully quitting every OpenCode process so the updated user-level plugin is loaded.
