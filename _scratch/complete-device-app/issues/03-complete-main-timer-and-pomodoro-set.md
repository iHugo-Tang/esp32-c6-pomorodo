# 03 — Complete the Main timer and Pomodoro Set

**What to build:** Deliver the complete Main experience so a user can move through Focus, Short break, and Long break, control an active timer, resolve Completion Transition, and see accurate Set Progress across a full Pomodoro Set.

**Blocked by:** 02 — Boot the first Focus on the device.

**Status:** ready-for-agent

- [ ] Ready, Running, Paused, and Completion Transition render the specified copy, actions, ring state, and Set Progress.
- [ ] Non-wrapping horizontal swipes change Mode; an active timer requires confirmation and an idle timer changes immediately.
- [ ] Pause, Resume, Reset confirmation, and aborted Mode changes preserve the domain rules and never count an Aborted Timer.
- [ ] Pomodoros 1–3 schedule Short break, Pomodoro 4 schedules Long break, and completing any Long break resets Set Progress.
- [ ] Auto-start, Start now, Cancel auto-start, and Not now follow Settings and never chain timers while powered off.
- [ ] Completion blocks navigation and Mode switching until the transition resolves.
