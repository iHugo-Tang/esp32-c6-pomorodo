# 03 — Migrate the device to Agent Status

**What to build:** Present and persist the device feature as Agent Status end to end, route all wire communication through the OC1 adapter, and remove the temporary OpenCode firmware surface without changing status or Pomodoro behavior.

**Blocked by:** 01 — Establish Agent Status and OC1 firmware seams.

**Status:** complete

- [x] Settings uses an `Agent` section with an `Agent status` switch that remains enabled by default.
- [x] Application actions, settings, view state, rendering objects, and state variables use Agent Status vocabulary.
- [x] Platform callbacks are named for reading and writing OC1 lines rather than for OpenCode.
- [x] Application code receives parsed OC1 state through the adapter and uses the adapter to format control and acknowledgement output; it contains no OC1 string construction.
- [x] Error, Waiting, Working, Completed, Idle, acknowledgement, five-second Completed display, ten-second heartbeat timeout, three-second control heartbeat, and display-wake behavior remain unchanged.
- [x] Agent Status changes do not alter Mode, the running Pomodoro, Set Progress, Statistics, or persistence outside the renamed setting.
- [x] Settings and top-level persisted-state versions are upgraded, and previous unreleased development formats load as empty storage rather than as a storage error.
- [x] Compatibility code for the previous Settings version is removed while unchanged Snapshot and Statistics formats are not spuriously versioned.
- [x] Legacy OpenCode firmware types, actions, modules, test targets, and compatibility entry points are removed after all callers migrate.
- [x] Agent Status, OC1, application, persistence, and all native host tests pass.

## Comments

- 2026-08-23: Migrated firmware, platform callbacks, settings, view state, UI objects, actions, and tests to Agent Status/OC1 vocabulary; removed the temporary OpenCode compatibility module and target. Settings is version 9 and persisted state is version 6; earlier development formats are treated as empty while Snapshot and Statistics stay at version 1. Native host tests (5/5) and a complete ESP-IDF build pass.
