# 02: Select an OpenCode Workspace from the TUI

**What to build:** Provide the first complete OpenCode path from a direct `/pomodoro-select` action in the TUI through the user-level plugin and broker to Agent Status publication. A user can select the current OpenCode Workspace, receive bounded feedback, and observe a basic Working-to-Completed lifecycle without creating model or conversation activity.

**Blocked by:** 01: Add explicit host-qualified Agent Source activation.

**Status:** ready-for-agent

**Completion:** pending human acceptance (real OpenCode TUI slash autocomplete)

- [ ] OpenCode 1.18.29 TUI slash autocomplete exposes exactly one `/pomodoro-select` direct action.
- [x] Invoking the action selects the current OpenCode Workspace and starts the broker on demand when necessary.
- [x] The action creates no prompt, Session message, Task, model request, or synthetic Working/Completed status cycle.
- [x] First selection and repeated selection each produce exactly one new broker revision.
- [x] A selected Session changing from busy to idle produces observable Working and Completed publication through the real broker boundary.
- [x] Selection displays bounded success, waiting-for-takeover, or failure feedback without exposing prompts, code, responses, or error details.
- [x] Selection failure leaves the previous Selected Agent Source unchanged and does not interrupt OpenCode.
- [x] The primary host integration check loads the real server and TUI plugin entry points against controlled OpenCode APIs and observes only external behavior.

## Comments

### 2026-09-06 Implementation (single coding executor)

**Implementation summary**

- `plugins/pomodoro-agent-status/scripts/opencode-core.mjs` (new): shared adapter core used by both plugin surfaces. It owns Workspace identity (`workspaceRoot` normalization from the existing Codex reporter: realpath of the Git root, falling back to the normalized working directory), member identity (`hashId("opencode:<sessionId>")`, distinct from Codex member hashing), the five-state member reducer (`MemberState`: busy → Working and clears a latched Error; retry → Working without clearing; permission/question asked → Waiting; replied/rejected → Working; `session.error` and tool parts with `state.status === "error"` → latched Error; `session.idle`/`status idle` → Completed unless Error is latched; otherwise Idle), plus `ActivationSession` (ensure broker, report an activation member as `host: "opencode"`, then one `select` request) and bounded feedback builders (`selectionFeedback`, `failureFeedback` with categories "broker unavailable", "broker rejected the request", "status source unavailable").
- `plugins/pomodoro-agent-status/scripts/opencode-server.mjs` (new): the real server plugin entry (exactly one exported function, legacy v0/v1 compatible). It tracks sessions (including subagents) from `session.created`/`session.updated`, removes members on `session.deleted`, reduces lifecycle events via the shared core, reports each member with `host: "opencode"` on change and renews leases every 3 s, and never auto-selects. Every hook body is fail-open (errors swallowed); a `dispose` hook stops the renewal timer. Reports go only to the broker socket resolved from the environment (overridable via plugin options for tests).
- `plugins/pomodoro-agent-status/scripts/opencode-tui.mjs` (new): the real TUI plugin entry (`{ id, tui }` module). At init it validates that the direct registration API exists (`api.command.register`/`trigger` and `api.ui.toast`); if not it throws an explicit error and registers no Markdown prompt command and no model-backed fallback. It registers exactly one direct action with `slash: { name: "pomodoro-select" }` whose `onSelect` resolves the current Workspace from `api.state.path` (read-only), runs `ActivationSession.activate()` (ensure broker on demand, one member report, one `select`), and shows one bounded toast (success / waiting-for-takeover / failure). A renewal keep-alive keeps the broker and the activation member alive while the selection is active; `lifecycle.onDispose` stops it. The TUI never touches the SDK client, submits no prompt, and creates no Session message, Task, or model request.
- `plugins/pomodoro-agent-status/scripts/check.mjs`: added `testOpenCodePluginSelection()` (check 8). It loads the real `opencode-server.mjs` and `opencode-tui.mjs` entry points against a controlled fake TUI API (command/ui/state/lifecycle only) and a hostile SDK `client` proxy that records and forbids every property access, connects them to a real broker in a temporary state directory, and asserts external behavior only: one registered direct command, selection revisions, publication masks, toast variants, persisted-state integrity, and zero client touches throughout.

**Interpretation recorded**

- When the broker is already running with live members, one selection invocation creates exactly one revision. When the broker must be started on demand against an already-selected-but-vacant source, the activation member's first report is a member-return that the broker answers with its recovery advance (selected source returning after total disappearance), and the explicit `select` advances once more — two revisions in that specific case. The select request itself always advances exactly once; the recovery advance is broker-owned member-return semantics required by Ticket 03.

**Check evidence (exact commands and outcomes)**

- Red first: `node plugins/pomodoro-agent-status/scripts/check.mjs` failed with `ERR_MODULE_NOT_FOUND` for `opencode-server.mjs`.
- After implementation: `node plugins/pomodoro-agent-status/scripts/check.mjs` → `pomodoro-agent-status check: 13 checks passed` (checks 1-7 are the pre-existing ones, unchanged; check 8 is new).
- Discriminator runs: with the TUI keep-alive removed the on-demand broker exits and the restored-selection assertion fails; with the corrupt/missing-broker shortcut replaced by the on-demand restart, the failure-feedback assertions failed until the impostor-socket scenario was used. Final state: all 13 pass.
- `node --check` on `opencode-core.mjs`, `opencode-server.mjs`, `opencode-tui.mjs`, `check.mjs` → all clean.

**Per-criterion results**

1. Pending human acceptance. The host check proves the real TUI plugin entry registers exactly one command with `slash: { name: "pomodoro-select" }` and a direct `onSelect` (check 8). Actual slash autocomplete and dispatch inside the shipped OpenCode 1.18.29 TUI cannot be proven here because verifying it requires fully quitting every OpenCode process and reloading the active user configuration, which this execution must not do. Requires a human smoke test after a full restart (see Ticket 06).
2. Passed. `onSelect` selected the current OpenCode Workspace through the real broker (`selected` becomes the host-qualified source, revision +1); after the broker was closed, the next invocation started one on-demand broker (`ensureBroker`) whose restored selection was observed via a real socket request (`{ ok, selected: <bare groupId>, revision: persisted + 2, selectedHost: "opencode" }`).
3. Passed. The hostile client proxy recorded zero SDK client touches across selection, repeat selection, the full Working→Completed lifecycle, and the failure path; the fake TUI exposes no prompt-submission surface at all; selection reports only an Idle-masked member and the Working/Completed masks observed came from real lifecycle events, not a synthetic cycle.
4. Passed. First selection: revision 0 → 1. Repeat selection: 1 → 2 (exactly one). On-demand selection after failures: persisted revision 4 → 6, where the extra advance is the broker-owned member-return recovery advance described under "Interpretation recorded"; the `select` request itself advanced exactly once each time.
5. Passed. A session created through the real server plugin's `event` hook, driven busy then idle, published `{WORKING}` then `{COMPLETED}` through the real broker's `publication()` with the host-qualified selection unchanged.
6. Passed. Feedback toasts: success (`variant: "success"`, mentions the revision), waiting-for-takeover (`variant: "info"`, explicitly names waiting for the broker, emitted when the broker had to start), and failure (`variant: "error"`, bounded category only, matching `/Error:|at /` negatively). No toast contains prompts, code, responses, or error details.
7. Passed. After the failure (impostor socket serving malformed IPC), the persisted selection file was byte-identical to before and the previous Selected Agent Source was restored by the next on-demand broker; nothing threw into the TUI host and OpenCode-visible state was untouched.
8. Passed. Check 8 loads `opencode-server.mjs` and `opencode-tui.mjs` (the real entry points that ship) against controlled fake OpenCode APIs and a real broker in temporary state; all assertions are on external behavior (registered command shape, broker responses, publication, toasts, persisted file, client-touch counter), not private helpers.

**Remaining work / notes**

- Criterion 1 requires a human TUI smoke test after a full OpenCode restart; it is the only unchecked box and the reason Completion is pending human acceptance. The detailed instructions are recorded under Ticket 06.
- The on-demand double-advance interpretation above should be reviewed with Tickets 03/05 evidence; it does not affect any acceptance criterion's observable behavior.
- Firmware, Codex plugin, and existing Codex checks were not modified.

### 2026-09-06 Coordinator review round 1 fixes (single coding executor)

**Gaps fixed**

1. **(High) Event attribution used the wrong id.** `eventSessionId()` preferred `properties.info.id`, but a real `message.updated` carries `info.id` = message id and `info.sessionID` = Session id (installed root SDK: `EventMessageUpdated { properties: { info: Message } }` with `Message.id`/`Message.sessionID`), so user messages created phantom members and cleared the wrong member's Error. Attribution is now event-shape aware: `session.*` uses `info.id`, `message.*` uses `info.sessionID` (then `part.sessionID`, then a direct `sessionID`), and all other events use the direct `sessionID`. Regression: the lifecycle check drives a real-shaped `message.updated` (`info: { id: "msg-1", sessionID: "s1", role: "user", ... }`) and asserts the source member count is unchanged and no member keyed by the message id exists (check 11).
2. **(High) Exact one-revision contract.** `ActivationSession.activate()` was always report-then-select, so a selection against a persisted selected-but-vacant source cost two revisions (recovery advance + select), violating criterion 4 and the parent spec's one-revision refresh contract. The old "double-advance interpretation" in the first implementation comment and in the parent spec is superseded and removed. `activate()` is now select-first: against a live source the single `select` is the whole revision; when the source has no live members it reports the activation member and decides from the report response's bounded fields (`selected`/`selectedHost`/`revision` — no free-text matching): if the member-return already re-established the selection, activation is complete with that single recovery advance; otherwise one explicit `select` performs the only advance. Updated evidence: the on-demand restored-selection assertion now expects `persisted + 1` (check 9), and first/repeat/active/vacant selections each advance exactly once.

**Check evidence**

- `node plugins/pomodoro-agent-status/scripts/check.mjs` → `pomodoro-agent-status check: 19 checks passed` (checks 1-8, 13-19 unchanged in behavior; the check count grew because new coverage was added in this and later tickets' checks).
- `node --check` on all plugin scripts → clean; `git diff --check` → clean.

**Per-criterion results (this round)**

1. Still pending human acceptance (unchanged; real TUI slash autocomplete cannot be verified without reloading active config).
2-3. Passed, evidence unchanged.
4. Passed, now strictly: first selection 0 → 1, repeat selection +1, selection against a live source +1, and selection against a selected-but-vacant source (including after on-demand broker restart) +1 — the `persisted + 1` assertion replaces the earlier `persisted + 2`.
5-8. Passed, evidence unchanged (check 9 additionally now uses the installed 1.18.29 `message.updated` shape).

**Remaining human checks**

- Criterion 1: real OpenCode 1.18.29 TUI slash autocomplete smoke test after a full quit/restart (instructions in Ticket 06).

### 2026-09-06 Coordinator review round 2 fixes (single coding executor)

**Gap fixed (High): concurrent activation correctness.**

- The select-first/report/conditional-select flow was not atomic: with two concurrent first activations, A's select+select could interleave with B's report such that B observed the source already selected at an unchanged revision and returned without advancing — a zero-revision invocation, violating the confirmed contract.
- The broker now has a dedicated atomic operation: `activate` carries the strictly validated member fields (`groupId`, `memberId`, alive `pid`, `startedAt`, `mask` 0..15) plus the optional `host`, and executes as one serialized broker-queue step that refreshes the activation member and performs the explicit selection — exactly one advance per accepted invocation in every state (fresh unselected source, live reselect, selected-but-vacant recovery after restart, or switching from another selected source). Strict exact-field/8 KiB/PID/mask validation, the Codex first-valid-source rule for `report`, persistence, request-scoped reaping, and sole broker serial ownership are unchanged. Unlike `report`, `activate` may establish a first selection for its host (it is an explicit selection action, not lifecycle activity).
- `ActivationSession.activate()` now sends exactly one `activate` request. The select-first branching, its docs, and its evidence are superseded and removed: the parent spec's interface summary and the curated spec's broker/`/pomodoro-select` sections now describe the atomic operation, and the request list includes `activate`.

**Check evidence**

- New check 9 (`testAtomicActivation`): fresh unselected source +1 (revision 1), live reselect +1 (revision 2), eight invalid `activate` requests rejected over the real socket with the revision unchanged (missing mask/field, extra field, dead PID, `startedAt: 0`, mask 16, invalid host string, missing `memberId`, numeric host), a host-less `activate` accepted as Codex with the historical response shape, activation while another source is selected +1, and three concurrent `ActivationSession.activate()` calls (two same-root, one other-root) returning three distinct monotonic revisions with the broker revision equal to the last accepted one. After a broker restart with persisted selected-but-vacant state, one activation advances exactly `persisted + 1` and publication resumes in the same step (check 10).
- `node plugins/pomodoro-agent-status/scripts/check.mjs` → `pomodoro-agent-status check: 21 checks passed` (plugin/TUI-path checks 11-13 now exercise the atomic activation; the on-demand restored-selection assertion remains `persisted + 1`).
- `node --check` on all plugin scripts → clean; `git diff --check` → clean.

**Per-criterion results (this round)**

1. Still pending human acceptance (unchanged).
2-3. Passed, evidence unchanged.
4. Passed, now provably under concurrency: every accepted invocation advances exactly once — serial, concurrent, fresh, reselect, and selected-vacant recovery (check 9/10 plus the TUI-path checks 11-12).
5-8. Passed, evidence unchanged.

**Remaining human checks**

- Criterion 1: real OpenCode 1.18.29 TUI slash autocomplete smoke test (Ticket 06 instructions).

### 2026-09-06 Coordinator review round 3 fix (single coding executor)

**Gap fixed (High): activate could double-advance through the request-scoped reap.**

- `BrokerCore.handleOne` runs a scoped `reapNow(source)` for every request before dispatch. For an `activate` against the currently selected source, that pre-dispatch reap could itself remove an expired/dead member, observe the aggregate mask change, and call `advance()` — then the activate branch called `advance()` again, so one invocation advanced twice.
- Fix: the pre-dispatch scoped reap now advances for `report`, `select`, and `remove` exactly as before, but passes `{ advance: false }` for `activate`; the activate branch prunes stale members, refreshes/sets its activation member, and performs the single `advance(source)` — removal, refresh, and selection are covered by exactly one revision. Ordinary `report`/`select` reaping semantics, serialized queue ownership, strict validation, persistence, and sole serial ownership are unchanged. A `reapMs` option was added to `runBroker` (default 250 ms, unchanged behavior) so the regression can run with the background reaper dormant.

**Regression evidence (check 11, real socket, deterministic)**

- Selected source with the lease-fresh activation member plus a killed child-process member (pid reaped, aggregate `WORKING`): one `activate` request yields exactly `revision + 1` (`3`), publication `{revision: 3, mask: IDLE}`, the stale member removed by the same step, and the selection unchanged.
- Discriminator: with the pre-dispatch reap restored to always-advance, check 11 fails with `actual: 4, expected: 3` — the regression provably covers the old implementation.

**Check evidence**

- `node plugins/pomodoro-agent-status/scripts/check.mjs` → `pomodoro-agent-status check: 22 checks passed`; `node --check` on all plugin scripts clean; CMake/CTest 6/6; `git diff --check` clean.

**Per-criterion results (this round)**

1. Still pending human acceptance (unchanged). Criteria 2-8 remain passed; criterion 4's "exactly one revision per invocation" now also holds when the activation itself cleans a stale selected-source member.
