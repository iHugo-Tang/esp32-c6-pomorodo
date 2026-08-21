# 08 — Complete direct pomodoro-select acceptance

**What to build:** Make the direct `/pomodoro-select` TUI command self-contained, current-API compliant, and fully verified so it switches the Selected OpenCode Process without invoking an LLM while preserving unrelated TPS work.

**Blocked by:** None — can start immediately.

**Status:** ready-for-human

- [x] The committed TUI configuration references only tracked plugins; unrelated TPS worktree changes remain intact and outside this fix unless separately reviewed and committed.
- [x] `/pomodoro-select` is registered through the current TUI keymap API without deprecated-command warnings.
- [x] Exactly one `/pomodoro-select` slash command remains, and executing it creates no Task, prompt, session message, or model request.
- [x] Runnable checks exercise the current command registration, direct current-process selection, missing and ambiguous identity, reselection, revision growth, and Toast outcomes.
- [ ] After fully quitting and restarting OpenCode, two processes in different projects verify autocomplete, no-steal startup, direct switching, serial handoff, fallback after exit, and zero LLM requests.
- [x] The restart-based acceptance evidence and any remaining limitations are recorded in the tracker.

## Comments

- 2026-08-21: Agent-verifiable acceptance complete. Implementation is committed in the user config repository (`61f34f8`, branch `main`). `tui.json` loads `./plugin/pomodoro-select.tsx` (tracked) plus `./plugin/tps.tsx`; the TPS files (`plugin/tps.tsx`, `plugin/tps-metric.ts`, `tests/tps.test.ts`) remain untracked and untouched, out of scope per this ticket. The old `command/pomodoro-select.md` and server `command.execute.before` hook are removed; `opencode debug config` shows no `pomodoroSelect` server command. `bun plugin/check-opencode-status.mjs` passes 120 checks (covers direct current-process targeting, missing and ambiguous process identity, reselection opening a new epoch, revision growth, no-steal startup, fallback after exit) and `bun plugin/check-pomodoro-select.mjs` passes 8 checks (covers command registration, direct selection without a model request, and success/failure Toast outcomes).
- Remaining limitation: the restart-based two-process acceptance cannot be completed from within a running OpenCode session — plugin and `tui.json` configuration load only at startup (see AGENTS.md), and verifying autocomplete, no-steal startup, direct switching, serial handoff, fallback after exit, and zero LLM requests requires fully quitting every OpenCode process, restarting two interactive TUIs in different projects, and observing them. This is a human step (see the unchecked item above).
