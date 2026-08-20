# 01 — Release the USB port safely for flashing

**What to build:** Provide one supported flashing flow that reserves the Pomodoro USB port, makes the OpenCode Status plugin release its reader and writer voluntarily, flashes with ESP-IDF, and always allows the plugin to reconnect afterward. The flow must not pause, terminate, or destabilize OpenCode.

**Blocked by:** None — can start immediately.

**Status:** complete

- [x] The flashing command creates a project-scoped reservation before opening the USB port.
- [x] The OpenCode Status plugin observes the reservation and closes both its reader process and serial write handle.
- [x] The plugin does not reconnect while the reservation exists.
- [x] Flashing waits for the plugin to release the port and fails clearly after a bounded timeout if another process still owns it.
- [x] Success, build failure, flash failure, interruption, and timeout all remove the reservation.
- [x] Removing the reservation allows the plugin to reconnect and resume OpenCode Status synchronization.
- [x] The flow never sends stop or termination signals to the OpenCode process.
- [x] Automated checks cover reservation acquisition, release, reconnect, and cleanup on successful and failed commands.
- [x] Existing OpenCode Status protocol checks and host tests continue to pass.
- [x] User documentation explains the supported flashing command and the required full OpenCode restart after installing the plugin change.
- [x] The USB/OpenCode Status ADR accurately records cooperative port reservation during firmware flashing.

## Comments

- 2026-08-20: Implemented. `tools/flash.sh` writes `.pomodoro/flash-reservation` (its PID) before opening the port, waits up to `POMODORO_FLASH_RELEASE_TIMEOUT_S` (default 8 s) for the OpenCode Status plugin to release the serial link via `lsof`, runs `idf.py flash`, and always removes the reservation via `trap` on success, failure, interruption, or timeout. The plugin polls the reservation every 500 ms, closes the reader and write handle on acquire, holds off reconnecting while reserved, and reconnects on release; a reservation whose PID is gone is treated as stale and released. ADR 0010, README, and `docs/HARDWARE_CONNECTION_TEST.md` record the flow.
- 2026-08-20: Verification — `tools/check-flash-reservation.sh` 3/3 checks (acquisition, cleanup on success, cleanup on flash failure, bounded-timeout failure + cleanup); `node .opencode/plugins/check-opencode-status.mjs` 32 checks including reservation transitions; host ctest 4/4 pass. Plugin file syntax checked with `node --check`. On-device behavior with a live OpenCode session is tracked separately in ticket 02.
