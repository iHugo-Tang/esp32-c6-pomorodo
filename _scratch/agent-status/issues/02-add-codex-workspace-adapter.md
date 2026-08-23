# 02 — Add the Codex Workspace adapter

**What to build:** A personal Codex plugin that maps trusted lifecycle hooks into broker reports, groups all chats and subagents by Codex Workspace, and exposes `$pomodoro-select` without changing the broker IPC schema or starting the broker.

**Blocked by:** 01 — Add the independent Agent Status broker.

**Status:** ready-for-human

- [x] The repository contains the single source for a valid `pomodoro-agent-status` plugin with no npm dependencies.
- [x] Each main session has a reporter with a short permission-restricted socket, three-second broker reports, live host-process cleanup, silent broker retry, and stale-socket recovery.
- [x] Git roots and non-Git working directories form stable Codex Workspace sources; chats, forks, side chats, and subagents aggregate through broker members.
- [x] Lifecycle hooks implement Idle, Working, Waiting, Completed, latched Error, compact preservation, structured tool failure, and subagent removal.
- [x] `$pomodoro-select` reports before selecting, returns a concise result, creates only the selection revision, and does not create a Working/Completed cycle.
- [x] Runnable checks cover pure mapping, real sockets, multi-session aggregation, Workspace isolation, broker absence/restart, malformed responses, and cleanup.
- [ ] CLI and Desktop manual acceptance is complete after hook trust and app restart.

## Comments

- 2026-08-23: Implemented and automated checks passed. Manual CLI/Desktop acceptance remains after installing, trusting the hooks with `/hooks`, and restarting Codex Desktop.
