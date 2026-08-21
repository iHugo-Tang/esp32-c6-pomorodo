# 02 — Add the project OpenCode Status plugin

> **已被取代**：项目级插件迁移到用户级配置，见 `_scratch/shared-opencode-status-plugin/`（spec 与 issue 01–06）。`OC1` 协议与设备行为不变。

**What to build:** Add a dependency-free project OpenCode plugin that maps current OpenCode events into the accepted aggregate status protocol, communicates over the selected macOS USB modem port, honors device `ENABLED`, `DISABLED`, and `ACK` messages, and never interrupts OpenCode when the device is absent or busy.

**Blocked by:** None — the wire contract is fixed in the accepted spec and ADR 0010, so this may proceed independently of issue 01.

**Status:** ready-for-agent

- [ ] The plugin aggregates all sessions in its current project and maps permission/question requests, busy/retry, idle, error, and deletion events according to the spec.
- [ ] The plugin sends `OC1 STATE` immediately after a meaningful aggregate change and every three seconds while the device reports `ENABLED`; heartbeats retain the same revision.
- [ ] Device acknowledgement clears only the matching Error or Completed revision and exposes the next aggregate priority without losing newer events.
- [ ] The plugin automatically uses the sole `/dev/cu.usbmodem*`; with multiple candidates it requires `POMODORO_PORT` and does not guess.
- [ ] Missing, disconnected, malformed, or exclusively occupied ports are handled silently with bounded periodic reconnect attempts and never fail an OpenCode task.
- [ ] `DISABLED` stops state writes; a later `ENABLED` immediately sends the current snapshot and resumes heartbeats.
- [ ] The plugin sends no prompt, code, response, title, error detail, or other session content and introduces no npm dependency or background service.
- [ ] A runnable plugin check covers event mapping, multi-session aggregation, priority, revision changes, heartbeat stability, acknowledgement, and enable/disable behavior without requiring hardware.
