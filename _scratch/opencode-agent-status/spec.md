---
title: OpenCode Workspace Agent Status
labels:
  - ready-for-agent
description: Add a user-level OpenCode TUI and lifecycle integration that explicitly selects an OpenCode Workspace and publishes its aggregate Agent Status through the existing broker.
---

## Problem Statement

Pomodoro can display the aggregate Agent Status of a selected Codex Workspace, but an OpenCode user cannot select an OpenCode Workspace or see its work, waiting, error, and completion states on the device. The historical OpenCode integration is not a viable solution because it used competing serial owners, automatic fallback, and an obsolete state layout that were intentionally replaced by the independent broker architecture.

The user needs a direct `/pomodoro-select` action in the OpenCode TUI that activates the current OpenCode Workspace without submitting a prompt, creating a Session message, or making a model request. Once selected, that Workspace must remain the only Agent Source that affects the device until another source is explicitly selected, even when Codex is active in the same repository.

## Solution

Add OpenCode as a second supported Agent Source type alongside Codex. A user-level OpenCode server plugin will translate lifecycle events from every live Session and subagent in an OpenCode Workspace into the existing five-state Agent Status model and report those members through the independent broker. A user-level OpenCode TUI plugin will expose `/pomodoro-select` as a direct action that selects the current OpenCode Workspace, starts the broker when necessary, and reports success or failure without invoking a model.

OpenCode Workspace and Codex Workspace identities remain distinct even when they refer to the same repository. Both hosts may continue reporting, but only the Selected Agent Source contributes the revision and mask published to the device. Selection persists when all members disappear and resumes when the same Workspace reports again; no other source takes over automatically.

The existing broker, USB ownership, flash reservation, `OC1` protocol, firmware state machine, and device UI remain authoritative and unchanged.

## User Stories

1. As an OpenCode TUI user, I want `/pomodoro-select` to select my current OpenCode Workspace, so that my device follows the work I intentionally chose.
2. As an OpenCode TUI user, I want the selection command to run without a model request, so that selecting the device source consumes no model time or tokens.
3. As an OpenCode TUI user, I want the selection command to create no prompt or Session message, so that control activity does not pollute my conversation.
4. As an OpenCode TUI user, I want `/pomodoro-select` to appear in slash autocomplete, so that I can discover and invoke it like other TUI commands.
5. As an OpenCode TUI user, I want a visible success response after selection, so that I know which Workspace now controls the device.
6. As an OpenCode TUI user, I want a visible waiting response while the broker is taking control, so that delayed device availability is understandable.
7. As an OpenCode TUI user, I want a non-blocking failure response when selection cannot complete, so that I can continue using OpenCode.
8. As an OpenCode TUI user, I want selection failures to preserve the previously Selected Agent Source, so that a failed command does not blank or unexpectedly switch the device.
9. As an OpenCode user, I want ordinary work before the first `/pomodoro-select` to leave source selection unchanged, so that a globally installed plugin cannot silently claim my device.
10. As an OpenCode user, I want selecting the same Workspace again to create a fresh revision, so that the device receives an explicit new selection even when the visible state is unchanged.
11. As an OpenCode user, I want busy and retrying work to display as Working, so that active processing is visible.
12. As an OpenCode user, I want permission requests and Question waits to display as Waiting, so that I know when my input is required.
13. As an OpenCode user, I want Session errors and structured tool failures to display as Error, so that failed work is not mistaken for completion.
14. As an OpenCode user, I want Error to remain latched until acknowledgement or real new work, so that transient lifecycle events do not conceal a failure.
15. As an OpenCode user, I want successful transition to idle to display Completed, so that I can notice finished work away from the terminal.
16. As an OpenCode user, I want acknowledged or expired completion to resolve to Idle, so that completed work does not remain permanently active.
17. As an OpenCode user, I want every live Session in the selected Workspace to participate in aggregation, so that concurrent work in the same repository is represented accurately.
18. As an OpenCode user, I want subagents to participate as separate live members, so that delegated work remains visible until it ends.
19. As an OpenCode user, I want Sessions in different OpenCode processes for the same Workspace to aggregate together, so that process boundaries do not fragment one Agent Source.
20. As an OpenCode user, I want Error, Waiting, Working, Completed, and Idle to retain the existing priority order, so that mixed member states resolve predictably.
21. As an OpenCode user, I want closing one Session to remove only that member, so that other work in the Workspace continues to control the aggregate.
22. As an OpenCode user, I want selection to remain when every Workspace member disappears, so that another source cannot take over without consent.
23. As an OpenCode user, I want the same Workspace to resume its selected status after OpenCode restarts, so that temporary process loss does not require an unintended source switch.
24. As a user running OpenCode and Codex in the same repository, I want them to remain distinct Agent Sources, so that selecting OpenCode excludes Codex status from the device aggregate.
25. As a user running OpenCode and Codex concurrently, I want selecting Codex after OpenCode to exclude OpenCode status, so that exactly one host source remains effective.
26. As an existing Codex user, I want the Codex selection and lifecycle behavior to remain unchanged, so that OpenCode support introduces no regression.
27. As a device owner, I want the independent broker to remain the only USB serial owner, so that multiple host integrations cannot contend for the device.
28. As a device owner, I want flashing reservation to continue releasing and restoring broker serial access, so that firmware updates remain safe while OpenCode is running.
29. As an OpenCode user, I want a missing broker, malformed IPC response, unavailable state directory, missing device, or occupied serial port not to interrupt OpenCode, so that status integration remains optional infrastructure.
30. As a privacy-conscious user, I want status and selection traffic to exclude prompts, code, responses, and error details, so that the device channel carries only bounded status metadata.
31. As a Pomodoro user, I want OpenCode status changes to leave Mode, the active Pomodoro, Set Progress, Statistics, and the countdown untouched, so that agent monitoring cannot alter productivity data.
32. As a user who works across projects, I want one user-level installation to make the integration available in every OpenCode Workspace, so that each project does not require a duplicate plugin.
33. As a maintainer, I want OpenCode 1.18.29 to be the minimum verified version, so that the supported lifecycle and TUI APIs have a concrete compatibility baseline.
34. As a maintainer, I want installation and upgrade checks to detect loss of the direct slash API, so that a deprecated interface cannot silently become a model-backed command.
35. As a maintainer, I want the authoritative plugin source and installation guidance to live with the Pomodoro integration, so that user-level deployment remains reproducible.
36. As a maintainer, I want host-side verification to exercise the real plugin boundaries and broker together, so that tests validate observable behavior rather than internal implementation details.

## Implementation Decisions

- The accepted Agent Source model includes both Codex Workspace and OpenCode Workspace. Host type is part of source identity, so equal normalized repository roots do not merge across hosts.
- A Git-backed OpenCode Workspace uses its normalized repository root as its boundary. A non-Git Workspace uses its normalized working directory.
- The OpenCode integration consists of a user-level server plugin, a user-level TUI plugin, and a shared adapter core. The shared core owns Workspace identity, member identity, lifecycle reduction, broker communication, activation state, and normalized selection results.
- The repository contains the authoritative plugin source and installation instructions. Installation is user-level and idempotent; the Pomodoro project must not load an additional project-level copy.
- The server plugin observes OpenCode lifecycle events and represents each Session and subagent as a member of its OpenCode Workspace. Members report a PID, process start identity, status mask, and lease renewal through the broker boundary.
- OpenCode lifecycle mapping is fixed: busy and retry map to Working; pending permission and Question interaction map to Waiting; Session errors and structured tool failures map to latched Error; successful idle maps to Completed; new, acknowledged-complete, and inactive members map to Idle. Real new work clears that member's latched Error.
- Workspace aggregation remains a bitwise union of live member masks, resolved as Error, Waiting, Working, Completed, then Idle. Existing acknowledgement and Completed-expiry behavior remains device-owned.
- The TUI plugin registers `/pomodoro-select` as a direct slash action using the OpenCode TUI plugin API available in 1.18.29. It must not define or silently fall back to a Markdown prompt command with the same name.
- The selection action identifies the current OpenCode Workspace, ensures at least its current member can participate, starts the broker on demand, and atomically establishes or refreshes selection with one new revision. Re-selecting the same Workspace also creates one new revision.
- Ordinary OpenCode lifecycle activity cannot become the first Selected Agent Source. OpenCode member reporting and activation coordination must preserve this rule even when the current broker behavior would otherwise auto-select its first valid report.
- Broker IPC remains strict, bounded JSON-lines over the protected local Unix socket. Atomic OpenCode activation is a dedicated broker operation: `activate` carries the same strictly validated member fields as `report` (`groupId`, `memberId`, alive `pid`, `startedAt`, `mask` 0..15) plus the optional `host`, and the broker performs it as one serialized queue step that refreshes the activation member and performs the explicit selection, so every accepted activation advances exactly one revision regardless of prior state (fresh unselected source, live reselect, selected-but-vacant recovery after restart, or switching from another source). Unknown, missing, extra, malformed, oversized, or invalid fields are rejected without affecting other clients.
- The broker contract accepts an optional `host` field on `report`, `select`, and `remove` requests; only `codex` (the default when the field is absent) and `opencode` are valid values. In-memory source keys are `host:groupId` for both hosts, so equal groupIds never merge across hosts, and any previously valid Codex `groupId` — including one that begins with `opencode:` — remains valid and cannot collide with an OpenCode source.
- Selection persists as `{selected, selectedHost, revision}` with the bare `groupId` and an explicit host. Legacy two-field state files (bare `selected`, no host) load as Codex selections and resume, so persisted selections from earlier brokers remain recoverable. Broker responses keep the historical contract: `selected` is the bare Workspace identity (null when nothing is selected) with no host discriminator for null or Codex selections; an OpenCode selection adds `selectedHost: "opencode"` to the response.
- Only Codex reports can establish the first selection; OpenCode reports update members and masks but never auto-select or advance the revision while their source is unselected. Request-scoped lease reaping is limited to the requesting source, so an unselected request can never trigger the selected source's lease-expiry advance; the background reaper owns expiry advances. Every accepted explicit selection advances exactly one revision, including selection of the already selected source; concurrent requests are serialized so each attempt advances exactly once.
- Selection state and monotonically increasing revision remain broker-owned and persist by atomic replacement. OpenCode members and masks remain ephemeral and rebuild from reports after restart.
- Codex and OpenCode may both retain live members, but the broker publishes only the Selected Agent Source. Reports from an unselected source cannot advance the device revision or alter its mask.
- A selected OpenCode Workspace remains selected after Session deletion, lease expiry, plugin disposal, OpenCode exit, or broker restart. The same host-specific Workspace identity resumes publication when a valid member returns; no other source automatically takes over.
- The independent broker remains the sole serial owner and continues to coordinate flash reservation, device discovery, retry, heartbeat, control state, ACK processing, and clean shutdown.
- The retained `OC1` transport and firmware behavior do not change. No OpenCode-specific data is added to the wire protocol.
- Plugin and command failures are fail-open for OpenCode use and fail-closed for device ownership. They return or display bounded status categories without prompts, code, responses, or error details.
- Command feedback has three observable outcomes: selected successfully, waiting for broker/device takeover, and failed without changing selection. Feedback uses the TUI surface and never creates a Session message.
- The first release supports selection only from the OpenCode TUI. Server lifecycle reporting may continue independently after selection, but Desktop, Web, and non-interactive CLI receive no promised selection command.
- OpenCode 1.18.29 is the minimum verified release. The direct slash registration API is already deprecated, so compatibility checks must fail explicitly if that API is unavailable rather than substituting model-backed behavior.
- Existing Codex behavior, device UI, Agent Status settings, Waiting Sound, wake behavior, state priority, ACK rules, heartbeat timeout, and flashing workflow remain unchanged.

## Testing Decisions

- The primary automated seam is the existing runnable host integration check for Agent Status. It will be extended rather than introducing a separate collection of low-level unit-test seams.
- The host integration check loads the real OpenCode server and TUI plugin entry points against controlled fake OpenCode APIs and connects them to a real broker running in a temporary protected state directory.
- Tests assert external behavior: broker publication and revision, Selected Agent Source identity, aggregate mask, member lifecycle, command feedback, and absence of prompt, message, Task, or model-request effects. Tests do not assert private helper calls or internal data layout.
- The harness drives busy, retry, permission, Question, Session error, structured tool failure, idle, new-work recovery, Session deletion, and subagent lifecycle events through the plugin boundary.
- The harness covers multiple Sessions and subagents in one Workspace, including members hosted by separate child processes, and verifies lease renewal, member-specific removal, total disappearance, and same-Workspace recovery.
- The harness creates OpenCode and Codex sources for the same normalized repository root and proves that only the selected host-specific source contributes to publication. Error or Waiting reports from the unselected host cannot change revision or mask.
- The harness proves that ordinary OpenCode events before first activation do not select OpenCode, first selection creates exactly one revision, and repeated selection creates exactly one additional revision without a synthetic status cycle.
- The harness covers simultaneous selection attempts and on-demand broker startup, preserving exclusive socket binding and one serial owner.
- Failure checks include missing broker, broker restart, malformed IPC, state-directory failure, stale or dead members, unavailable device, occupied port, and plugin disposal. OpenCode must remain usable and prior selection must remain intact where required.
- Installation checks verify that both user-level plugin surfaces are active, slash autocomplete exposes `/pomodoro-select`, no same-name Markdown command exists, and two different projects use one installation without duplicate loading.
- Prior art is the existing Agent Status host check, which already exercises the real broker, reporter, temporary sockets and state directories, child processes, Workspace isolation, lifecycle reduction, selection revisions, recovery, permissions, and on-demand startup.
- A real OpenCode 1.18.29 TUI smoke test remains required because a fake TUI cannot prove actual slash discovery, direct dispatch, Toast rendering, or the absence of a submitted Session message in the shipped TUI.
- Physical-device acceptance remains required to verify visible five-state behavior, source switching, USB exclusivity, disconnect recovery, and flash reservation. Host checks and firmware builds cannot prove physical hardware behavior.
- Existing Codex host checks remain regression coverage and must continue to pass without weakening strict IPC validation or first-valid-source behavior for Codex.

## Out of Scope

- Direct selection from OpenCode Desktop, Web, or non-interactive CLI.
- Publishing the integration as an npm package or authorizing remote publication.
- A project-level OpenCode plugin or duplicate project-local installation.
- Automatically selecting OpenCode from ordinary lifecycle activity before `/pomodoro-select`.
- Automatically falling back to another OpenCode Workspace, Codex Workspace, Session, or process when the selected source disappears.
- Aggregating OpenCode and Codex into one Agent Source when they refer to the same repository.
- Device-side source browsing or source selection.
- Changes to the device UI, Agent Status colors, priority, acknowledgement, Completed expiry, heartbeat timeout, Waiting Sound, wake behavior, or settings.
- Changes to the `OC1` protocol, firmware Agent Status state machine, USB discovery, or flash reservation semantics.
- Direct serial ownership by OpenCode or restoration of the historical competing-process broker design.
- Transporting prompts, code, responses, error details, Pomodoro commands, settings, statistics, or firmware data through Agent Status.
- Migration, inspection, or deletion of the historical OpenCode state directory.
- Compatibility guarantees for OpenCode releases older than 1.18.29.

## Further Notes

- Delivery status: Ticket 01 (host-qualified Agent Source activation) is accepted. Tickets 02-04 are implemented and evidence-backed by the extended host integration check (`node plugins/pomodoro-agent-status/scripts/check.mjs`, 22 checks) and remain pending coordinator review. Ticket 05 is implemented with the same automated evidence except that its installation-validation criterion requires the real OpenCode TUI to confirm the direct slash API, so it is pending human acceptance. Ticket 06 documentation is updated (product doc, curated spec, README); the real OpenCode 1.18.29 TUI smoke test and all physical-device criteria remain pending human acceptance, so the shared specification stays incomplete and retains the `ready-for-agent` label.
- This specification implements the accepted architecture in ADR-0016 and preserves ADR-0012, ADR-0013, and ADR-0014. ADR-0015 is superseded.
- The confirmed requirements handoff is the source for stable requirement identifiers `OCAS-R001` through `OCAS-R014`, acceptance identifiers `OCAS-AC001` through `OCAS-AC017`, and failure scenarios `OCAS-F001` through `OCAS-F007`.
- The implementation should proceed through the confirmed candidate slices: establish the cross-host source model, add the OpenCode lifecycle adapter and host checks, add the direct TUI selection path, verify shared broker isolation and recovery, then align product documentation and perform hardware acceptance.
- The TUI command API's deprecated status is an explicit delivery risk, not permission to weaken the zero-model command contract. If the direct API is removed before implementation completes, return to design rather than silently changing user-visible behavior.
- Local spec publication does not authorize npm, GitHub, or any other remote publication.
- Verified interface summary (Tickets 02-05): the server plugin is a single exported function receiving `{ client, project, directory, worktree, serverUrl, $ }` and returning `{ event, dispose }` hooks (the `Hooks` typing shipped alongside the current installation includes `dispose`); the TUI plugin is a `{ id, tui }` module whose `tui(api, options)` registers exactly one command with `slash: { name: "pomodoro-select" }` and a direct `onSelect`. Both surfaces resolve the broker socket from the environment (overridable through plugin options for tests) and never touch the SDK client. Selection feedback is one bounded toast with three outcomes. Activation sends one atomic `activate` request (member refresh + selection in one broker-queue step), so every accepted invocation — serial or concurrent, fresh, reselect, or selected-vacant recovery — advances exactly one revision. The installer CLI is `node plugins/pomodoro-agent-status/scripts/install-opencode.mjs install|uninstall|verify [--config-dir DIR] [--project-dir DIR] [--opencode PATH] [--version X.Y.Z]`; installer validation covers the version floor, plain-JSON configuration integrity (it refuses JSONC configurations and malformed files before any mutation), both canonical project duplicate directories (`.opencode/plugin/` and `.opencode/plugins/`), and file/registration identity, while the direct slash API is validated when the TUI plugin initializes after a full restart.
- Delivery truth: "delivered" in the tickets and canonical docs means implemented and covered by the automated host checks; the real OpenCode TUI smoke test (slash discovery, dispatch, Toast, absence of prompt/message/Task/model effects) and all physical-device criteria are mandatory acceptance steps that remain pending human execution. Automated evidence does not substitute for them, and no doc claims that validation occurred.
