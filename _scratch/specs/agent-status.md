# Agent Status

## Source and selection

An Agent Source is one Codex Workspace. A Git repository uses its normalized repository root as the Workspace boundary; a non-Git directory uses its normalized working directory. Chats, forks, side chats, and subagents within that boundary are members of the same source.

The device displays one Selected Agent Source. The first valid source becomes selected. After that, only an explicit selection changes it. If the selected source disappears, the selection remains and the device waits for that Workspace to return; another source never takes over automatically. Selecting or reselecting a Workspace starts a new revision even when its visible status is unchanged.

`$pomodoro-select` selects the current Codex Workspace. It first reports the source, starts the broker if necessary, and performs only the selection revision; the control turn must not create a Working or Completed cycle.

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

The removed client dimension is invalid. Unknown operations, extra or missing fields, invalid identifiers, dead or invalid PIDs, invalid timestamps, masks outside `0..15`, malformed JSON, and oversized lines reject only that request.

A report renews a ten-second member lease. Dead PIDs and expired members are removed. A Workspace mask is the bitwise OR of its live member masks. The broker persists only the selected Workspace identity and monotonically increasing revision by atomic replacement. Members and masks are rebuilt from reports after restart. Corrupt or unsafe state fails closed and is not overwritten.

The Codex adapter reports every three seconds from a lightweight reporter whose live PID carries the lease. It removes the member when the host exits. Broker absence or malformed responses do not block Codex; the reporter retains state and retries. Lifecycle events map ordinary prompt and tool work to Working, permission and user-input requests to Waiting, structured tool failure to latched Error, a successful stop to Completed, and session start to Idle. Compaction does not reset status. Real new work clears a latched Error. Subagents join and leave as separate members.

Only selection starts a detached broker when no listener exists. Concurrent starts converge through exclusive socket binding. An automatically started broker exits after ten seconds without a valid Agent report; selection, removal, invalid IPC, and device heartbeats do not extend that lifetime. A manually started foreground broker remains until interrupted.

`SIGINT` and `SIGTERM` close serial and socket resources while preserving the selected Workspace and revision.

## USB and flashing

The broker is the only USB serial owner. The retained wire protocol uses bounded newline-terminated `OC1` ASCII lines:

- Host to device: `OC1 STATE <revision> <mask>`.
- Device to host: `OC1 ENABLED`, `OC1 DISABLED`, and `OC1 ACK <revision> <mask>`.

The device adapter rejects malformed, unknown-version, overflowing, oversized, or out-of-range input. The broker publishes immediately after `ENABLED` and repeats the same revision every three seconds. `DISABLED` stops publication. An ACK is accepted only for the currently published revision and only for Error or Completed bits present in the raw mask.

`POMODORO_PORT` may select the port. Otherwise automatic discovery succeeds only when exactly one `/dev/cu.usbmodem*` exists. Missing, occupied, or disconnected ports retry silently with a bounded interval.

A live-PID `flash-reservation` in the Agent Status state directory makes the broker close and stop reopening the serial port. Flashing waits a bounded time for release and removes its reservation after success, failure, timeout, or interruption. It never signals or terminates Codex. Communication resumes after the reservation is removed. The `OC1` channel carries status only; it never transports prompts, code, responses, error details, Pomodoro commands, settings, statistics, or firmware updates.

## OpenCode compatibility

OpenCode Process is not a supported Agent Source. The project has no project-level OpenCode status plugin, and OpenCode cannot publish status, select a source, or own the device serial port.

Runtime code does not inspect, migrate, or delete the historical OpenCode state directory. An operator may remove it only after stopping the old broker. The `OC1` wire protocol remains because current Agent Status still uses it; the name does not imply OpenCode support.

## Related requirements

The audible notification for entering Waiting is owned by [Sound and audio feedback](sound.md#waiting-sound).
