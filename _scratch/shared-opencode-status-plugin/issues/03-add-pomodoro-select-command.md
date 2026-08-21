# 03 — pomodoro-select command with feedback

**What to build:** The `/pomodoro-select` command available in every OpenCode process: running it in a target OpenCode switches the selected process to that instance, publishes a new state epoch to the device immediately, and confirms success with a TUI toast plus a short session reply. It must not alter Timer, Pomodoro Set, Statistics, or other device behavior.

**Blocked by:** 02 — Add the global plugin wiring.

**Status:** ready-for-agent

- [ ] `/pomodoro-select` is available as a global command in every OpenCode process and, when run in a non-selected process, atomically re-selects that process.
- [ ] Selection switches open a new state epoch even when the aggregate color is unchanged, clearing the previous source's locally acknowledged states and waking the device.
- [ ] Success is confirmed with a TUI toast and a short session reply; running it in the already-selected process still opens a new state epoch and reports the refreshed selection.
- [ ] The command and its hook are implemented inside the existing plugin and introduce no npm dependency or background service.
