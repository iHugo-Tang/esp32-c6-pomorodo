# 04: Isolate OpenCode and Codex source switching

**What to build:** Make concurrent OpenCode and Codex use safe and predictable from the user's perspective. Both hosts may remain alive and report for the same repository, but selecting either host makes it the only Agent Source that affects device status, without introducing another USB owner or weakening flashing coordination.

**Blocked by:** 03: Aggregate complete OpenCode Workspace lifecycle.

**Status:** ready-for-agent

**Completion:** accepted by coordinator

- [x] With Codex Waiting and OpenCode Working in the same repository, selecting OpenCode publishes only Working.
- [x] Selecting Codex afterward publishes only Waiting, and later OpenCode reports cannot alter the selected revision or mask.
- [x] Error, Waiting, or recovery reports from either unselected host remain isolated from device publication.
- [x] Concurrent source selections are serialized and leave one deterministic Selected Agent Source with monotonic revisions.
- [x] The independent broker remains the only process that opens the device serial port while both hosts are active.
- [x] Flash reservation releases broker serial access and restores communication afterward without stopping or signaling OpenCode or Codex.
- [x] Existing Codex selection, lifecycle, Workspace aggregation, and on-demand broker checks continue to pass.
- [x] Host integration coverage proves cross-host isolation through public plugin and broker behavior rather than duplicated reducer-only tests.

## Comments

### 2026-09-06 Implementation (single coding executor)

**Implementation summary**

- No production code changes were required: the host-qualified broker contract delivered in Ticket 01 and the two real plugin surfaces delivered in Ticket 02/03 already provide the isolation semantics. This ticket's work is the integration evidence.
- `check.mjs`: added `testCrossHostPluginIsolation()` (check 10). It runs the real Codex reporter (`startReporter`, real local hook socket, real `$pomodoro-select` control turn) and the real OpenCode server plugin against the same normalized repository root, connected to one real broker whose serial publisher is driven through a controlled fake port and transport that records every open and every written line.

**Check evidence (exact commands and outcomes)**

- Red first: the initial version failed twice — the Codex member's own state machine replaces (rather than accumulates) its single member mask, so the expected `WORKING|WAITING` aggregate was actually `WAITING`, and the fake transport never sent `OC1 ENABLED` so no `OC1 STATE` line was written. Both were test-expectation fixes, not production changes; the corrected scenario matches the ticket literally (Codex is Waiting when OpenCode is selected, and Codex is Waiting again when re-selected).
- After implementation: `node plugins/pomodoro-agent-status/scripts/check.mjs` → `pomodoro-agent-status check: 15 checks passed` (check 10 new; checks 1-9 and 11-15 unchanged).

**Per-criterion results**

1. Passed. Codex was left Waiting (work then `PermissionRequest`) and the OpenCode session was driven busy; selecting OpenCode through the real activation path published exactly `{revision +1, mask WORKING}` — no Codex bits.
2. Passed. Selecting Codex through the real reporter control turn published exactly `{revision +1, mask WAITING}`; afterwards the OpenCode session's `session.error` latch and retry reports changed only the unselected OpenCode member while the selected publication stayed `{revision, WAITING}`.
3. Passed. While OpenCode was selected, the unselected Codex member latched an Error (`PostToolUse` exit 2 + `Stop`) and then recovered to Waiting (new work clearing the latch, then a permission wait); publication remained `{WORKING}` throughout. The mirrored direction is criterion 2. "Waiting reports from an unselected host" are additionally covered by the earlier `{WAITING}` isolation phase.
4. Passed. Two concurrent `select` requests (one OpenCode, one Codex) both succeeded with revisions exactly 1 apart, serialized by the broker queue, leaving one deterministic Selected Agent Source and monotonic revisions (the raw-protocol concurrency evidence from Ticket 01's check 6 is unchanged and still passes).
5. Passed. Both hosts report only over their Unix-socket IPC; the fake serial port records every open, and only the broker's publisher ever opened it (opens counter attributed solely to the broker; neither plugin has any serial code path — verified by the hostile client/IPC boundaries in checks 8-9 and by the passing line recording here).
6. Passed. Writing a live-PID `flash-reservation` file made the broker close its transport within one tick and not reopen it while held; both hosts kept reporting and publication kept resolving during the reservation; removing the file made the broker reconnect and resume `OC1 STATE` publication. Neither host process was signaled or stopped.
7. Passed. Checks 1-5 and 11-15 (Codex reducer, hook selection, reporter integration, legacy persistence, recovery, on-demand broker) are unchanged and pass; no existing check was weakened.
8. Passed. All cross-host evidence flows through the real plugin boundaries (Codex hook socket, OpenCode `event` hook, activation session) and the real broker socket/serial publisher — the reducer is never asserted directly in this test.

**Remaining work / notes**

- The serial transport here is a controlled fake; real-device serial behavior remains a physical-device criterion (Ticket 06).
- "The broker remains the only serial owner" is additionally guaranteed architecturally: neither new module imports any serial capability, and the check records every open of the fake port.

### 2026-09-06 Final coordinator acceptance

- Independent review of `bb759b05a4633be4fcda24563f22acdb28f3e32b...d6da696` returned Standards PASS and Spec PASS with zero findings.
- The coordinator independently reran `node plugins/pomodoro-agent-status/scripts/check.mjs`; all 22 checks passed, including cross-host selection isolation, serialized concurrent activation, sole serial ownership, flash reservation, and unchanged Codex behavior.
- The coordinator independently reran the documented CMake configure/build flow and `ctest --test-dir build/host --output-on-failure`; all 6 host tests passed. All eight plugin scripts passed `node --check`, the isolated installer round trip passed, and `git diff --check` was clean.
- Criteria 1-8 are accepted as passed at the host-integration boundary. Ticket 06 correctly retains real-device serial ownership and flash recovery as pending physical-device acceptance rather than claiming that controlled transport proves hardware behavior.
- The canonical tracker vocabulary has no completed triage role, so `Status: ready-for-agent` is retained and completion is recorded separately above.
