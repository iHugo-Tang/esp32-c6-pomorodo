# 01: Add explicit host-qualified Agent Source activation

**What to build:** Extend the broker-facing source-selection behavior so an OpenCode Workspace can be explicitly activated without ordinary OpenCode lifecycle reports claiming an unselected device. OpenCode Workspace and Codex Workspace identities must remain distinct even for the same normalized repository, while existing Codex first-source and explicit-selection behavior continues unchanged.

**Blocked by:** None (can start immediately).

**Status:** ready-for-agent

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
