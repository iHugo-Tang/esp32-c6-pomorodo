# 02 — Add the Codex Workspace adapter

**What to build:** A personal Codex plugin that maps trusted lifecycle hooks into broker reports, groups all chats and subagents by Codex Workspace, and exposes `$pomodoro-select` without changing the broker IPC schema. Selection starts a ten-second-idle broker on demand.

**Blocked by:** 01 — Add the independent Agent Status broker.

**Status:** ready-for-human

- [x] The repository contains the single source for a valid `pomodoro-agent-status` plugin with no npm dependencies.
- [x] Each main session has a reporter with a short permission-restricted socket, three-second broker reports, live host-process cleanup, silent broker retry, and stale-socket recovery.
- [x] Git roots and non-Git working directories form stable Codex Workspace sources; chats, forks, side chats, and subagents aggregate through broker members.
- [x] Lifecycle hooks implement Idle, Working, Waiting, Completed, latched Error, compact preservation, structured tool failure, and subagent removal.
- [x] `$pomodoro-select` reports before selecting, returns a concise result, creates only the selection revision, and does not create a Working/Completed cycle.
- [x] `$pomodoro-select` reuses an existing broker or starts the plugin broker on demand; only valid Agent reports extend its ten-second idle lifetime.
- [x] Runnable checks cover pure mapping, real sockets, multi-session aggregation, Workspace isolation, broker absence/restart, malformed responses, and cleanup.
- [ ] CLI and Desktop manual acceptance is complete after hook trust and app restart.

## Comments

- 2026-08-23: Implemented and automated checks passed. Manual CLI/Desktop acceptance remains after installing, trusting the hooks with `/hooks`, and restarting Codex Desktop.
- 2026-08-23: Fixed selection after Codex submitted `$pomodoro-agent-status:pomodoro-select` while the reporter and hook only recognized `$pomodoro-select`. Both forms now share one predicate across control-state handling, timeout, and result output. The real namespaced hook check selected the Workspace with exactly one revision; a fresh installed CLI thread returned revision 2, and the connected ESP32 showed the Agent Status indicator. Captured `_scratch/agent-status/codex-status-acceptance.png`. Desktop acceptance remains pending.
