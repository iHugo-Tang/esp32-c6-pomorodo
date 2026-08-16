# 01 — Establish a testable application orchestrator

**What to build:** Create one application-level seam that can run the Pomodoro product against injected platform services. From that seam, a host test must be able to boot the application, provide user actions and time updates, observe the resulting screen state, and capture storage, display, and audio effects without ESP32 hardware.

**Blocked by:** None — can start immediately.

**Status:** resolved

- [x] A host test can boot with no saved state and observe Focus in Ready state with default Settings.
- [x] A host test can press Start, advance absolute time to the deadline, and observe exactly one Focus completion event and Completion Transition.
- [x] User input, clock updates, rendered view state, and requested side effects cross a single documented application boundary.
- [x] Platform failures are surfaced as deterministic application states or effects rather than hidden global behavior.
- [x] Existing core, statistics, persistence, migration, and torn-write tests remain passing.
