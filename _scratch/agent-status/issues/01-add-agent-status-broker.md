# 01 — Add the independent Agent Status broker

**What to build:** A foreground, dependency-free Node CLI broker that owns Agent Status selection and aggregation, local JSON-line IPC, persistent revision/selection, OC1 serial publication, flashing reservation, and reconnect behavior without changing the existing OpenCode plugin.

**Blocked by:** None — can start immediately.

**Status:** ready-for-agent

- [x] The executable `tools/pomodoro-agent-status-broker.mjs` enforces strict 8 KiB JSON-line requests over a permission-restricted, single-instance Unix socket.
- [x] Reports use ten-second member leases and live PIDs; each source is isolated and aggregates member masks with bitwise OR.
- [x] First-source selection, explicit-only switching, retained missing selection, recovery, and revision rules match the spec without automatic fallback.
- [x] Only `{selected, revision}` is atomically persisted; restart requires fresh reports and corrupt state fails closed without overwrite.
- [x] The selected live source alone drives three-second OC1 heartbeats, `ENABLED`/`DISABLED`, scoped ACK acceptance, port retry/reconnect, and `flash-reservation` release.
- [x] SIGINT/SIGTERM close the socket and serial transport while preserving selection and revision.
- [x] `tools/check-agent-status-broker.mjs` covers the pure core, real Unix socket, persistence, CLI lifecycle, and controllable serial seam without hardware.

## Comments

- 2026-08-23: Implemented the standalone broker and runnable check. OpenCode and Codex adapters remain separate follow-up issues.
