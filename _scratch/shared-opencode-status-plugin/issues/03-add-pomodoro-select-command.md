# 03 — pomodoro-select command with feedback

**What to build:** The `/pomodoro-select` command available in every OpenCode process: running it in a target OpenCode TUI switches the selected process to that instance, publishes a new state epoch to the device immediately, and confirms success with a TUI toast. It executes directly from the TUI plugin without invoking an LLM or creating a session message (see 07). It must not alter Timer, Pomodoro Set, Statistics, or other device behavior.

**Blocked by:** 02 — Add the global plugin wiring.

**Status:** ready-for-agent

- [x] `/pomodoro-select` appears in TUI slash autocomplete and, when run in a non-selected process, atomically re-selects that process without a model request.
- [x] Selection switches open a new state epoch even when the aggregate color is unchanged, clearing the previous source's locally acknowledged states and waking the device.
- [x] Success is confirmed with a TUI toast; running it in the already-selected process still opens a new state epoch and reports the refreshed selection.
- [x] The command is registered by the TUI plugin (`plugin/pomodoro-select.tsx`) and introduces no npm dependency or background service.
