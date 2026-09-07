# 03: Aggregate complete OpenCode Workspace lifecycle

**What to build:** Expand the selected OpenCode Workspace from a single happy-path Session into the complete Agent Source model. Every live Session and subagent in the Workspace contributes its five-state lifecycle across process boundaries, while member cleanup and Workspace recovery preserve explicit selection.

**Blocked by:** 02: Select an OpenCode Workspace from the TUI.

**Status:** ready-for-agent

**Completion:** pending coordinator review

- [x] Busy and retrying members contribute Working; pending permission and Question interaction contribute Waiting.
- [x] Session errors and structured tool failures contribute latched Error, successful idle contributes Completed, and inactive members contribute Idle.
- [x] Real new work clears only the affected member's latched Error, while ordinary idle or stop events do not conceal it prematurely.
- [x] Workspace aggregation resolves Error, Waiting, Working, Completed, then Idle across all live members.
- [x] Sessions and subagents in separate OpenCode processes but the same Workspace aggregate into one OpenCode Agent Source.
- [x] Session deletion, subagent completion, dead processes, and lease expiry remove only the affected member.
- [x] When all members disappear, the Workspace remains selected and no other source takes over automatically.
- [x] A valid member returning with the same Workspace identity restores publication with a new revision after OpenCode or broker restart.
- [x] The host integration check drives the complete lifecycle through plugin boundaries, real broker IPC, temporary state, and child processes.

## Comments

### 2026-09-06 Implementation (single coding executor)

**Implementation summary**

- `opencode-core.mjs`: `MemberState` already implements the fixed lifecycle mapping; this ticket's evidence comes from driving the real server plugin entry through it. Mapping details: `session.status` `busy` → Working (clears that member's latched Error), `retry` → Working without clearing, `idle` → Completed unless Error is latched; `session.idle` → Completed; `session.error` → latched Error; `permission.asked` and `question.asked` → Waiting; `permission.replied`, `question.replied`, `question.rejected` → Working; `message.updated` with a user message → Working and clears the latched Error; `message.part.updated` tool parts with `state.status "error"` → latched Error, with `pending`/`running` → Working. Members start Idle when created and are Idle again only via device-side acknowledgement/expiry (the broker mask keeps COMPLETED, matching the Codex contract). No changes were needed beyond Ticket 02's reducer.
- `opencode-server.mjs`: unchanged this ticket; subagents are OpenCode sessions with `parentID` and already join as separate members keyed by their own session ID, so subagent completion/deletion is the same member-removal path as any session.
- `check.mjs`: added `testOpenCodeLifecycle()` (check 9) plus helpers (`fakeServerInput`, `loadServerPlugin`, `spawnOpenCodeProcess`); `waitFor` gained an optional failure message. The check drives two main sessions and a subagent through the real server plugin `event` hook, connects them to a real broker in temporary state, spawns a real child Node process that reports a member for the same Workspace (its own PID carrying the lease), kills it, and verifies removal; it also disposes the plugin and waits out the lease for total vacancy and recovery.

**Check evidence (exact commands and outcomes)**

- Red first: `node plugins/pomodoro-agent-status/scripts/check.mjs` failed while the reducer lacked retry-without-clearing and the child process never reported (args were lost to `-e` argv slicing); both were fixed (retry mapping was already correct; the child driver moved its parameters to environment variables).
- After implementation: `node plugins/pomodoro-agent-status/scripts/check.mjs` → `pomodoro-agent-status check: 14 checks passed` (check 9 new; checks 1-8 unchanged).

**Per-criterion results**

1. Passed. busy and retry on s1/s3 plus `permission.asked` on s2 produced publication `{WORKING|WAITING}`; `question.asked`/`replied` transitions were also driven (s2 Waiting then back to Working).
2. Passed. Tool parts with `state.status "error"` and `session.error`-equivalent latching produced Error; `session.idle` on s2 produced Completed; freshly created, event-less members contribute Idle (s3 contributed `0` until its first event; the initial aggregate before any event is IDLE).
3. Passed. After s1 latched Error, s2's idle to Completed did not conceal it; a user message on s1 (`message.updated`) cleared only s1's Error and left no ERROR bit among members; a second tool error on s1 latched again and survived the other members' activity.
4. Passed. The published aggregate mask is the bitwise OR of live member masks (e.g. `{ERROR|COMPLETED|WORKING}`, `{ERROR|WAITING|WORKING}`), and the priority order Error > Waiting > Working > Completed > Idle is resolved from that mask by the existing firmware state machine, which is unchanged and covered by `pomodoro_agent_status_test` in the host suite.
5. Passed. A real child Node process (separate PID, separate process) reporting the same `groupId` with `host: "opencode"` aggregated into the same source; `SIGKILL` of that process removed only its member after lease/PID cleanup, leaving the two in-process members' aggregate untouched.
6. Passed. `session.deleted` for s3 (the subagent) removed only that member; the killed child process member was removed only by its own death; plugin disposal stopped lease renewal so every remaining member expired through the background reaper.
7. Passed. After total vacancy, publication became null while `selected` stayed the host-qualified OpenCode source, and a Codex report for the same `groupId` was accepted without taking over (first selection is Codex-gated and the source is not null).
8. Passed. A fresh server plugin instance reporting the same Workspace identity restored publication (`{WORKING}`) with a new revision (asserted greater than the selection revision) while the selection stayed the same host-qualified source.
9. Passed. The whole scenario runs through the real plugin entry point's public `event` hook, real broker JSON-lines IPC over the real Unix socket, a temporary state directory, and a real child process; assertions are on broker responses, publication, member counts, and the persisted selection — not on reducer internals (the reducer itself is only exercised indirectly through the plugin boundary).

**Remaining work / notes**

- Member removal on plugin disposal relies on the 10 s lease (OpenCode 1.18.29 exposes no server-plugin `dispose` guarantee in the installed typing; a `dispose` hook is provided and additionally stops renewal eagerly when the host calls it).
- Firmware/device behavior is unchanged; no hardware criterion belongs to this ticket.

### 2026-09-06 Self-review fix (single coding executor)

Self-review finding: the server plugin ignored lifecycle events for sessions it had never seen a `session.created` for — notably a session resumed after a full OpenCode restart, which never replays `session.created`. Such a member could never exist, so a resumed session could not contribute to the Workspace aggregate or to recovery publication.

Fix: `opencode-server.mjs` now lazily creates the member for any lifecycle event whose session is unknown, attributing it to the process's Workspace (`baseRoot`) until a `session.updated` corrects the group. Regression evidence: `testOpenCodeLifecycle()` now drives `session.status` for an unseen `s-resumed` session during the recovery phase and asserts it joins as a second member of the same Workspace; `node plugins/pomodoro-agent-status/scripts/check.mjs` → 17 checks passed; the CTest host suite passes unchanged.
