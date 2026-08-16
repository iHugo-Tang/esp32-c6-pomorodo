# 04 — Recover a timer without data loss

**What to build:** Persist the complete product state through the existing dual-slot NVS journal so an unexpected restart restores Settings, timer state, Set Progress, and statistics without double-counting or discarding the last recoverable state.

**Blocked by:** 03 — Complete the Main timer and Pomodoro Set.

**Status:** ready-for-agent

- [ ] State is loaded during boot and defaults are saved only when no valid supported state exists.
- [ ] Running timers before their deadline resume from the RTC deadline; paused timers retain their exact remaining duration.
- [ ] A timer whose deadline passed while powered off settles exactly once and returns in Completion Transition without starting the next Mode.
- [ ] State saves occur on transitions and confirmed Setting changes, never once per second.
- [ ] A torn or corrupt latest slot falls back to the previous valid slot, and supported persisted schema versions migrate without losing user data.
- [ ] Host integration tests exercise save, reboot, restore, corruption, and idempotent settlement through the application boundary.
