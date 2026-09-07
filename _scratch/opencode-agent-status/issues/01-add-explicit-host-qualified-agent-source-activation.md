# 01: Add explicit host-qualified Agent Source activation

**What to build:** Extend the broker-facing source-selection behavior so an OpenCode Workspace can be explicitly activated without ordinary OpenCode lifecycle reports claiming an unselected device. OpenCode Workspace and Codex Workspace identities must remain distinct even for the same normalized repository, while existing Codex first-source and explicit-selection behavior continues unchanged.

**Blocked by:** None (can start immediately).

**Status:** ready-for-agent

**Completion:** accepted by coordinator

- [x] A host-qualified OpenCode Workspace identity cannot collide or aggregate with the Codex Workspace identity for the same normalized repository root or working directory.
- [x] Ordinary OpenCode lifecycle activity before `/pomodoro-select` does not become the Selected Agent Source or advance the device revision.
- [x] First activation and reactivation of an OpenCode Workspace each create exactly one new revision, including under concurrent activation attempts.
- [x] Reports from an unselected source cannot change the selected publication revision or mask.
- [x] Strict bounded IPC validation rejects malformed, oversized, unknown, missing, extra, or invalid activation data without affecting other clients.
- [x] Existing Codex first-valid-source, explicit selection, persistence, aggregation, and recovery checks remain unchanged and pass.
- [x] The runnable host integration check verifies the new behavior through the real broker boundary rather than private helper assertions alone.

## Comments

### 2026-09-06 Implementation (single coding executor)

**Implementation summary**

- `plugins/pomodoro-agent-status/scripts/broker.mjs`: requests `report`, `select`, and `remove` now accept an optional strict `host` field (`codex` default when absent, `opencode`). Validation still requires the exact field set per type, keeps the 8 KiB line bound, and additionally rejects unknown host values, non-string/null host, Codex requests whose `groupId` starts with `opencode:` (namespace collision guard), and OpenCode groupIds too long for the namespaced persisted key. `BrokerCore` keys OpenCode sources as `opencode:<groupId>` (Codex keys stay bare for persisted-state compatibility), and only Codex reports can establish the first selection (`firstSelection` is host-gated). Explicit selection semantics are unchanged otherwise: live members required, one revision per accepted `select`, serialized by the existing per-broker queue, so concurrent attempts each advance exactly once. No changes to `OC1`, `SerialPublisher`, persistence schema, socket handling, or firmware.
- `plugins/pomodoro-agent-status/scripts/check.mjs`: added `testOpenCodeActivation()` (checks 6-7). It drives a real broker started with `runBroker()` over its real Unix socket with raw JSON-lines (`request()`), including invalid requests, an over-8-KiB line, concurrent activations on separate connections, and a broker restart against the same state directory. Assertions use broker responses (`ok`, `revision`, `selected`) and the publication the serial publisher consumes (`publication()`); the new behavior is not tested through private helpers alone. No existing check functions were modified.
- `reporter.mjs`, `hook.mjs`, skills, and firmware are untouched; the Codex reporter keeps working because `host` defaults to `codex`.

**Check evidence (exact commands and outcomes)**

- Red first: with only the new check wired in, `node plugins/pomodoro-agent-status/scripts/check.mjs` failed at `testOpenCodeActivation` (`false !== true`: the broker rejected `host` as an extra field).
- After implementation: `node plugins/pomodoro-agent-status/scripts/check.mjs` → `pomodoro-agent-status check: 10 checks passed`.
- `cmake -S test -B build/host` → configured; `cmake --build build/host --parallel` → built; `ctest --test-dir build/host --output-on-failure` → `100% tests passed out of 6` (including `pomodoro_agent_status_test` and `pomodoro_oc1_protocol_test`, confirming firmware-side behavior is unchanged).

**Per-criterion results**

1. Passed. Same `groupId` (same normalized root) is reported by both hosts: selecting Codex publishes only `{WAITING}` and selecting OpenCode publishes only the OpenCode aggregate `{ERROR|WORKING}`; the response to activation reports `selected` as `opencode:<groupId>`. A Codex request with an `opencode:`-prefixed `groupId` is rejected, so the namespaces cannot merge.
2. Passed. Two OpenCode reports before any selection leave `selected` null, revision 0, publication null; a later ERROR report while Codex is selected leaves publication `{revision 1, WAITING}` unchanged.
3. Passed. First activation: revision 1 → 2 (exactly one). Reactivation of the same Workspace: 2 → 3 (exactly one). Two concurrent activations on separate connections: both ok, revisions 4 and 5, serialized by the broker queue, exactly one advance per attempt. Interpretation recorded: "each create exactly one new revision" is implemented per accepted activation attempt (spec also requires re-selection of the same Workspace to create one new revision, which rules out deduplicating repeat selections to a no-op).
4. Passed. Unselected-source reports never advance: OpenCode ERROR report with Codex selected → publication `{1, WAITING}`; Codex report after restart with OpenCode selected → publication `{9, IDLE}`.
5. Passed. Ten invalid requests rejected through the real socket (extra field, unknown host, numeric host, null host, missing `groupId`, unknown request type, unexpected report field, report missing `mask`, oversized `groupId`, Codex `groupId` with `opencode:` prefix) plus an over-8-KiB line (`request line exceeds 8 KiB`); selection and revision unchanged after all rejections and subsequent valid requests still succeed.
6. Passed. Checks 1-5 and 8-10 are byte-identical to before (verified in the diff: only a new function and its invocation were added) and all pass.
7. Passed. New coverage speaks the real broker protocol over the real socket, including concurrency and restart; private-state reads are limited to observable getters (`selected`, `revision`, `publication()`).

**Remaining problems / notes**

- OpenCode selection persists as `opencode:<groupId>`; previously persisted Codex selections (bare `groupId`) remain loadable and resumable, so the upgrade path keeps Codex recovery intact.
- Completion is pending coordinator review; this comment records implemented-and-checked state only. The shared spec was made more precise (broker `host` field, namespaced OpenCode keys, host-gated first selection) but is not marked complete, and Tickets 02-06 are untouched.
- TUI/plugin, real OpenCode 1.18.29 smoke, and physical-device validation remain outside this ticket (Tickets 02-06); no hardware check was required or performed here.

### 2026-09-06 Independent review findings (completion back to pending review)

Independent code review against `1117f5c9e268e9627198553c7286b494bc983f78` returned Standards PASS, Spec FAIL with two findings. Completion is recorded as pending review again until both are fixed and reinspected.

1. **(Medium) Unselected requests can trigger the selected source's expiry advance.** `handleOne()` ran a full `reapNow()` for every request, so lease/PID expiry of a *selected* member could call `advance()` while servicing an *unselected* OpenCode report — that request would change the selected publication revision/mask, violating criterion 4.
2. **(Low) Ambiguous prefix namespace and persistence.** Rejecting Codex groupIds beginning with `opencode:` changed the previously valid Codex ID contract, and persisted selection had no host discriminator, so the `opencode:` string prefix was ambiguous with legacy bare Codex selections.

Both findings are being addressed in the next commit; criteria will be re-evidenced there.

### 2026-09-06 Review fixes (single coding executor)

**Fixes (commit 2)**

- Finding 1 (`broker.mjs`): request handling now reaps only the requesting source (`reapNow(scope)`); the background reaper (250 ms tick) remains the sole owner of full lease cleanup and of the selected source's expiry advance. A request from an unselected source therefore cannot trigger an advance of the selected publication. Lease cleanup and selected-source recovery/mask-change behavior are preserved: the scoped reap still prunes the requesting source's dead members (so `select` cannot pick a dead-only source and recovery detection stays correct), and a report from the selected source still advances its own publication when a sibling expires. Verified as a real-socket regression (`testUnselectedExpiryIsolation`): with the old full reap the new test fails (the unselected OpenCode report produced `{revision 3, mask WORKING}` instead of the unchanged `{2, WORKING|WAITING}`); with the fix the report leaves publication untouched and the background reaper performs exactly the one expiry advance.
- Finding 2 (`broker.mjs`): the Codex `opencode:`-prefix rejection and the OpenCode groupId length restriction were removed, so every previously valid Codex groupId (up to the existing 128-char limit) is valid again. Internal source keys are now `host:groupId` for both hosts. Persisted selection is `{selected, selectedHost, revision}` with the bare groupId and explicit host; legacy two-field files (bare `selected`, no host) load as Codex selections and resume, including legacy groupIds that begin with `opencode:`. Regression evidence: `testOpenCodeActivation` now reports and selects a Codex source with groupId `opencode:<hash>` alongside the OpenCode source with `<hash>` — no collision or aggregation (selecting the odd Codex source publishes only its own `{WORKING}`); the restart block asserts the persisted file is `{selected, selectedHost, revision}` with `selectedHost: "opencode"` and the bare groupId, and resumes; `testLegacyPersistedSelection` writes a legacy two-field file whose selected value starts with `opencode:` and proves the broker resumes it as a Codex selection.

**Check evidence (exact commands and outcomes)**

- `node plugins/pomodoro-agent-status/scripts/check.mjs` → `pomodoro-agent-status check: 12 checks passed` (was 10; adds the expiry-isolation and legacy-persistence checks).
- Discriminator runs: with `reapNow()` temporarily restored to unscoped, the new expiry check failed as the finding predicts; restored to `reapNow(source)`, all 12 pass.
- `cmake -S test -B build/host` → configured; `cmake --build build/host --parallel` → built; `ctest --test-dir build/host --output-on-failure` → `100% tests passed out of 6` (all six listed and executed).
- `git diff --check` → clean.

**Per-criterion results (after fixes)**

1. Passed. Same-`groupId` sources stay distinct via `host:groupId` keys; the `opencode:`-prefixed Codex groupId coexists with the OpenCode source without collision or aggregation, and both directions of selection publish only the selected host's aggregate.
2. Passed. OpenCode reports before activation leave selection null, revision 0, publication null; unselected OpenCode reports (including while Codex is selected) never advance.
3. Passed. First activation +1, reactivation +1, two concurrent attempts serialized to exactly one advance each; the expiry-isolation check additionally shows an unselected request cannot inject an extra advance.
4. Passed. Unselected reports cannot change the selected revision or mask — including the lease-expiry case: the request that would previously have advanced the selected source now leaves publication untouched, and the background reaper performs exactly one legitimate expiry advance.
5. Passed. Nine invalid activation requests plus the over-8-KiB line are rejected through the real socket; selection and revision unchanged; subsequent valid requests succeed. (The previously rejected prefixed Codex groupId is now a positive coexistence case instead.)
6. Passed. Existing check functions 1-5 and 10-12 remain unchanged and pass; the only pre-existing assertion adjusted is `broker.core.selected` in the new Ticket 01 test itself (Codex keys are now `codex:<groupId>`).
7. Passed. All new behavior is exercised through the real broker socket and the persisted state file, including concurrency, restart, legacy-file loading, and expiry timing.

**Remaining problems / notes**

- Completion remains pending coordinator review; the shared spec's broker interface/persistence bullets were updated to the verified behavior and are not marked complete. Tickets 02-06 remain untouched.
- Supersede note: this comment supersedes the "Implementation (single coding executor)" comment above where it describes the Codex `opencode:`-prefix rejection, bare Codex internal keys, and the OpenCode groupId length restriction — those were removed by the review fixes.
- In-memory keys and the `selected` field in broker responses are `host:groupId`; the persisted file keeps the bare `groupId` plus `selectedHost`. TUI/plugin, real OpenCode 1.18.29 smoke, and physical-device validation remain outside this ticket.

### 2026-09-06 Coordinator acceptance

- Acceptance conclusion: passed. The coordinator independently inspected the complete task diff from starting commit `1117f5c9e268e9627198553c7286b494bc983f78` through implementation commit `1a02d93` and found no scope, contract, or documentation gap for Ticket 01.
- `node plugins/pomodoro-agent-status/scripts/check.mjs` independently returned `pomodoro-agent-status check: 10 checks passed`, including the real-socket OpenCode activation, isolation, validation, concurrency, and persisted-recovery scenarios.
- `cmake -S test -B build/host`, `cmake --build build/host --parallel`, and `ctest --test-dir build/host --output-on-failure` independently completed with all 6 tests passing.
- Criteria 1-7 are accepted as passed based on the implementation comment's per-criterion evidence and the independently inspected assertions. No criterion is failed, blocked, or pending human acceptance.
- TUI and physical-device checks remain assigned to later tickets and do not limit completion of this broker-foundation ticket. The shared specification remains incomplete.
- The canonical tracker vocabulary has no completed triage role, so the `ready-for-agent` triage label is retained and completion is recorded separately above rather than misusing `ready-for-human`.
