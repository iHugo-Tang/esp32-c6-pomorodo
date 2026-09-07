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
