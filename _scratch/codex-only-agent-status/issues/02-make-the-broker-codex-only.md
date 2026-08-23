# 02 — Make the broker Codex-only

**What to build:** Make Codex Workspace the only Agent Source throughout the broker, reporter, plugin, and local tooling, using the new Agent Status state directory while preserving selection, aggregation, serial publication, and flashing behavior.

**Blocked by:** None — can start immediately.

**Status:** complete

- [x] Report requests identify a Workspace group and member without a client field.
- [x] Select requests identify only a Workspace group, and remove requests identify a Workspace group and member.
- [x] Responses expose the Selected Agent Source as a Workspace group identity or null.
- [x] Requests carrying the removed client field are rejected by exact-field validation rather than silently accepted.
- [x] Persisted coordination contains only the selected Workspace group identity and revision; corrupt state still fails closed.
- [x] Workspace isolation, member leases, mask aggregation, explicit selection, recovery, and revision rules remain unchanged.
- [x] The broker, reporter, plugin, foreground entry, flash reservation flow, and checks use `$XDG_STATE_HOME/pomodoro/agent-status`, with the documented default under the user's local state directory.
- [x] Runtime code neither migrates nor deletes the old OpenCode state directory.
- [x] OC1 ENABLED/DISABLED, heartbeat, ACK, reservation, retry, disconnect, and serial ownership behavior remain unchanged.
- [x] Broker, plugin, real-socket, on-demand lifecycle, and flash-reservation checks pass with the new schema and directory.

## Comments

- 2026-08-23: Removed the broker `client` dimension, reduced Selected Agent Source persistence and responses to a Workspace group identity, and moved all runtime coordination to `$XDG_STATE_HOME/pomodoro/agent-status`. Broker checks (5/5), plugin checks (8/8), and flash-reservation checks (10/10) pass.
