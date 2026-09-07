# Agent Status

## Source and selection

An Agent Source is one host-qualified Workspace: a Codex Workspace or an OpenCode Workspace. A Git repository uses its normalized repository root as the Workspace boundary; a non-Git directory uses its normalized working directory. Host type is part of the source identity, so a Codex Workspace and an OpenCode Workspace for the same path never merge. Chats, forks, side chats, and subagents within that boundary are members of the same source.

Both the Codex and the OpenCode integrations are delivered. Codex is selected with `$pomodoro-select`; OpenCode is selected with the direct `/pomodoro-select` action in the OpenCode TUI, provided by a user-level server plugin (lifecycle reporting) and a user-level TUI plugin (selection), both sharing one adapter core. The broker contract accepts host-qualified sources and explicit OpenCode activation; ordinary OpenCode activity can neither publish status into a first selection nor become selected.

The device displays one Selected Agent Source. The first valid Codex report establishes the selection; OpenCode reports cannot become the first selected source. After that, only an explicit selection changes it. If the selected source disappears, the selection remains and the device waits for that Workspace to return; another source never takes over automatically. Selecting or reselecting a Workspace starts a new revision even when its visible status is unchanged.

`$pomodoro-select` selects the current Codex Workspace. It first reports the source, starts the broker if necessary, and performs only the selection revision; the control turn must not create a Working or Completed cycle.

`/pomodoro-select` is a direct OpenCode TUI action registered through the deprecated-but-available `api.command.register` slash API of OpenCode 1.18.29. It resolves the current OpenCode Workspace from the TUI path, starts the broker on demand, reports one activation member, and issues one `select` request. It submits no prompt, creates no Session message, Task, or model request, and produces no synthetic status cycle. Its feedback is one bounded toast: selected successfully, waiting for broker/device takeover, or failed with the previous selection preserved. If the direct slash API is unavailable, TUI plugin initialization fails explicitly and no same-name Markdown command or model-backed fallback is created. First-release selection is TUI-only; OpenCode Desktop, Web, non-interactive CLI, and npm publication are out of scope.

## Status model

The broker aggregates all live members of the selected Workspace. The visible priority is:

1. Error — `#FF5C5C`
2. Waiting — `#FFD166`
3. Working — `#5B8CFF`
4. Completed — `#4DD17A`
5. Idle — `#454B56`

The Main page shows the resolved state as a 28 px circle at the bottom left, 26 px from the left edge and vertically aligned with the Sound button. Its transparent touch target is 44×44 px. It participates in pixel shift and all four UI Orientations. It is hidden outside Main and behind blocking dialogs.

Only Error and Completed can be acknowledged. An acknowledgement applies only to the current revision and reveals the next active priority without changing the broker's aggregate mask. Completed returns to Idle after five seconds. Error persists until it is acknowledged, cleared by new work, or disconnected. Ten seconds without a valid enabled heartbeat hides the indicator.

A meaningful externally supplied status change wakes the display, including a display the user turned off. Repeated heartbeats, local acknowledgement, Completed expiry, and connection timeout do not wake it again. Agent Status never changes Mode, the active Pomodoro, Set Progress, Statistics, or the countdown.

Settings contains an `Agent` section with a default-on, persisted `Agent status` switch. Off hides the indicator, ignores incoming status, prevents status-triggered wake, and publishes the disabled control state. On waits for a fresh synchronization rather than showing stale state.

Firmware keeps Agent Status behavior separate from wire parsing. The Agent Status module owns enabled and connected state, revision, aggregate mask, local acknowledgement, Completed expiry, heartbeat expiry, and pending display wake. It accepts parsed revision, mask, and time values and never accepts or produces `OC1` text. A concrete OC1 adapter exclusively parses STATE input and formats ENABLED, DISABLED, and ACK output; application code routes through it instead of constructing protocol strings.

## Broker and lifecycle adapter

A foreground Node broker with no npm dependencies owns source selection, member aggregation, revision allocation, USB serial access, and flashing coordination. Adapters report through a local Unix socket; they never own the serial port.

The broker socket and state live under `${XDG_STATE_HOME:-$HOME/.local/state}/pomodoro/agent-status/`. The directory uses mode `0700` and the socket uses `0600`. JSON-line requests are limited to 8 KiB and must contain exactly the fields for their operation:

- `report`: Workspace group identity, member identity, PID, start time, and mask.
- `select`: Workspace group identity.
- `remove`: Workspace group identity and member identity.

Every request may also carry one optional, strictly validated `host` field: only `codex` (the default when absent) and `opencode` are accepted. Internal source keys are `host:groupId` for both hosts, so equal groupIds never merge across hosts and every previously valid Codex `groupId` — including one beginning with `opencode:` — remains valid without colliding with an OpenCode source.

The removed client dimension is invalid. Unknown operations, extra or missing fields, invalid identifiers or hosts, dead or invalid PIDs, invalid timestamps, masks outside `0..15`, malformed JSON, and oversized lines reject only that request.

A report renews a ten-second member lease. Dead PIDs and expired members are removed. Request-scoped reaping covers only the requesting source; the background reaper owns full cleanup and the selected source's expiry advance, so an unselected request can never advance the selected publication. A Workspace mask is the bitwise OR of its live member masks. The broker persists only the selected Workspace identity with an explicit host (`selected`, `selectedHost`, `revision`) and the monotonically increasing revision by atomic replacement; legacy state files with a bare `selected` and no host load as Codex selections and resume. Members and masks are rebuilt from reports after restart. Corrupt or unsafe state fails closed and is not overwritten.

Responses keep the historical contract: `selected` is the bare Workspace identity (null when nothing is selected) and carries no host discriminator for null or Codex selections; an OpenCode selection adds `selectedHost: "opencode"`.

The Codex adapter reports every three seconds from a lightweight reporter whose live PID carries the lease. It removes the member when the host exits. Broker absence or malformed responses do not block Codex; the reporter retains state and retries. Lifecycle events map ordinary prompt and tool work to Working, permission and user-input requests to Waiting, structured tool failure to latched Error, a successful stop to Completed, and session start to Idle. Compaction does not reset status. Real new work clears a latched Error. Subagents join and leave as separate members.

The OpenCode server plugin observes the OpenCode event stream (including `question.asked`/`question.replied`/`question.rejected` in 1.18.29) and reports each Session and subagent as a separate member of its Workspace with `host: "opencode"`. Its lifecycle mapping is fixed: busy and retry map to Working (busy clears that member's latched Error, retry does not), pending permission and Question interaction map to Waiting and answering returns the member to Working, `session.error` and tool parts in the `error` state map to latched Error, successful idle maps to Completed, and new or inactive members map to Idle. Every hook body fails open: broker, IPC, state-directory, device, or disposal failures never interrupt OpenCode, and failures surface only as bounded selection feedback that preserves the prior selection. Plugin removal relies on the broker's ten-second lease and PID cleanup.

Only selection starts a detached broker when no listener exists. Concurrent starts converge through exclusive socket binding. An automatically started broker exits after ten seconds without a valid Agent report; selection, removal, invalid IPC, and device heartbeats do not extend that lifetime. A manually started foreground broker remains until interrupted.

`SIGINT` and `SIGTERM` close serial and socket resources while preserving the selected Workspace and revision.

## USB and flashing

The broker is the only USB serial owner. The retained wire protocol uses bounded newline-terminated `OC1` ASCII lines:

- Host to device: `OC1 STATE <revision> <mask>`.
- Device to host: `OC1 ENABLED`, `OC1 DISABLED`, and `OC1 ACK <revision> <mask>`.

The device adapter rejects malformed, unknown-version, overflowing, oversized, or out-of-range input. The broker publishes immediately after `ENABLED` and repeats the same revision every three seconds. `DISABLED` stops publication. An ACK is accepted only for the currently published revision and only for Error or Completed bits present in the raw mask.

`POMODORO_PORT` may select the port. Otherwise automatic discovery succeeds only when exactly one `/dev/cu.usbmodem*` exists. Missing, occupied, or disconnected ports retry silently with a bounded interval.

A live-PID `flash-reservation` in the Agent Status state directory makes the broker close and stop reopening the serial port. Flashing waits a bounded time for release and removes its reservation after success, failure, timeout, or interruption. It never signals or terminates Codex. Communication resumes after the reservation is removed. The `OC1` channel carries status only; it never transports prompts, code, responses, error details, Pomodoro commands, settings, statistics, or firmware updates.

## OpenCode compatibility and installation

The historical OpenCode Process integration is not a supported Agent Source and is not restored. The delivered OpenCode Workspace integration is the broker-based one: a user-level server plugin reports lifecycle, a user-level TUI plugin provides `/pomodoro-select`, and the broker remains the sole serial owner. OpenCode 1.18.29 is the minimum verified release; `install-opencode.mjs install|verify` validates that floor (from `opencode --version` or an explicit version) and fails explicitly below it. The TUI plugin additionally validates the direct slash registration API at startup and fails explicitly without it, never registering a Markdown prompt command or model-backed fallback.

Installation is user-level and idempotent: the authoritative files are copied into `<configDir>/plugins/pomodoro-agent-status/` and the server surface is registered in `opencode.json` `plugin` while the TUI surface is registered in `tui.json` `plugin`. Configuration changes require fully quitting every OpenCode process and restarting. `verify` re-checks file identity, both registrations, and — for a project directory — that no duplicate project-level copy exists; the Pomodoro repository itself must not load one. `uninstall` removes both entries and the installed files without touching broker-persisted selection state. Disabling without uninstalling is documented via `tui.json` `plugin_enabled`.

Runtime code does not inspect, migrate, or delete the historical OpenCode state directory. An operator may remove it only after stopping the old broker. The `OC1` wire protocol remains because current Agent Status still uses it; the name does not imply OpenCode support.

## Related requirements

The audible notification for entering Waiting is owned by [Sound and audio feedback](sound.md#waiting-sound).
