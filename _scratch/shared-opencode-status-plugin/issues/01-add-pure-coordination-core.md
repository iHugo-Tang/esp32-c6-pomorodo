# 01 — Pure coordination core for process selection

**What to build:** A dependency-free, hardware-free coordination core in the user-level OpenCode configuration that turns multiple OpenCode processes into a single selected state source: instance registration, first-instance auto-selection, explicit re-selection, fallback on exit or heartbeat timeout, stale record cleanup, a shared monotonically increasing wire revision, and the global USB flashing reservation protocol. Existing event aggregation and priority rules move into this core unchanged.

**Blocked by:** None — can start immediately.

**Status:** ready-for-agent

- [x] Instances register in the user-level state directory with a random instance ID, PID, and start time; the first registered instance becomes the selected process and later instances register without taking over.
- [x] Re-selection atomically replaces the selected process and always opens a new state epoch (new wire revision), independent of whether the aggregate color changes.
- [x] When the selected instance heartbeats time out or exits, the earliest still-alive instance takes over; with no alive instance, selection is empty and the device is disconnected.
- [x] Stale instance records are pruned and concurrent election races resolve to exactly one selected instance.
- [x] The global revision increments on selection changes and on aggregate state changes of the selected instance, and stays stable across heartbeats; ACKs only affect the currently published revision.
- [x] The global flashing reservation protocol acquires and releases based on a marker file with a live PID, and the core fails closed when the state directory cannot be created, written, or locked.
- [x] A runnable, dependency-free check covers event mapping, priority aggregation, first-instance selection, no-steal behavior, explicit reselection, fallback, stale cleanup, revision stability and increments, ACK scoping, reservation transitions, and fail-closed paths.

- 2026-08-20: Implemented. `plugin/opencode-status-core.mjs` adds a strictly validated, snapshot-backed `createCoordinator` (registration, first-instance selection, explicit reselection, timeout/exit fallback to earliest alive, stale pruning, monotonic global revision), a fail-closed `transactCoordinator` boundary for storage adapters to lock and atomically commit shared state, and an instance-bound `createProtocol` that blocks non-selected publication and scopes event/ACK/reselect/enable lines to the published revision; `createAggregator` and `createReservation` move in unchanged. `plugin/check-opencode-status.mjs` runs 101 checks covering all criteria, including shared snapshot handoff, selected-instance enforcement, contended election, revision/time boundaries, and lock/read/write/missing-field/inconsistent-state failures. Renamed to `.mjs` so the plugin loader skips it; `opencode debug config` confirms it is not registered as a plugin origin.
