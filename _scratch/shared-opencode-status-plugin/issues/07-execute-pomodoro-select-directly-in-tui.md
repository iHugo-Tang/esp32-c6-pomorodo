# 07 — Execute pomodoro-select directly in the TUI

**What to build:** Replace the prompt-based `/pomodoro-select` command with a TUI plugin command that directly selects the current OpenCode Process, opens a new state epoch, and reports the result without invoking an LLM or creating session messages.

**Blocked by:** 03 — pomodoro-select command with feedback.

**Status:** ready-for-human

- [x] `/pomodoro-select` appears in TUI slash autocomplete and executes plugin code directly without creating a Task, prompt, session message, or model request.
- [x] Selecting or reselecting the current OpenCode Process is atomic, increments the shared revision, clears previous local acknowledgement state, and wakes the device even when the aggregate color is unchanged.
- [x] The TUI displays success, pending-handoff, and failure Toast feedback without requiring a session reply.
- [x] The old Markdown prompt command and server command hook are removed, leaving exactly one `/pomodoro-select` registration.
- [x] Runnable checks cover current-process targeting, missing or ambiguous process identity, reselection, and revision behavior.
- [x] After fully restarting OpenCode, two processes verify autocomplete, no-steal startup, direct switching, fallback, and zero LLM requests.
- [x] The feature specification, ticket 03, and architecture decision describe the direct TUI command path.

## Comments

- 2026-08-21: Implemented in the user config repository. `plugin/pomodoro-select.tsx` registers `/pomodoro-select` as a TUI plugin command (`tui.json` `plugin` entry); `onSelect` atomically selects the instance matching `process.pid` via the new `selectCurrentProcess`/`findInstanceForPid` core helpers, then reports success/pending-handoff/failure with a TUI toast. Storage helpers (state dir, lock, transaction, serial-owner handoff poll) moved to `plugin/opencode-status-state.mjs` shared by the server plugin and the TUI command; the server plugin's `command.execute.before` hook and `command/pomodoro-select.md` are removed. `plugin/check-opencode-status.mjs` gains targeting/ambiguity/reselection/revision checks (120 checks), and `plugin/check-pomodoro-select.mjs` loads the real TUI plugin with a mocked api against a temp state dir (8 checks). `opencode debug config` shows no `pomodoroSelect` server command; `bun test` passes 18 tests. Remaining: the restart-based two-process acceptance above (config changes load only at startup).
