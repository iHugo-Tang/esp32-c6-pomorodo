- [x] OpenCode 1.18.29 TUI slash autocomplete exposes exactly one `/pomodoro-select` direct action.

### 2026-09-06 Real-TUI round fix: broker child host runtime (single coding executor)

**Bug reproduced by the coordinator in the real OpenCode 1.18.29 TUI** (tmux): `/pomodoro-select` appeared in autocomplete and dispatched, but selection failed with the bounded "broker unavailable" feedback and no broker process/socket ever started. Root cause: `ensureBroker()` spawned `process.execPath`, which is Node under Codex and the host checks but is the compiled OpenCode/Bun binary (`/opt/homebrew/bin/opencode`) inside the TUI — that executes `opencode <broker.mjs> --idle` and exits, so no broker can ever start from the TUI. The prior fake-API acceptance missed this host-runtime difference; the real TUI found it.

**Fix (minimal, no unnecessary abstraction):** new `brokerExecutable(env, execPath)` in `reporter.mjs`: an explicit `POMODORO_BROKER_NODE` override wins; otherwise a Node host keeps spawning its own executable (existing Codex/test behavior byte-for-byte); a non-Node host resolves `node` from PATH. `ensureBroker` spawns that executable. No reliance on `process.execPath` inside a non-Node host.

**Regression (closest seam, independent of the test process being Node)**

- Unit: `brokerExecutable` pins override-wins, non-Node-execPath → `node`, Node-execPath → itself (check 3).
- Integration: `ensureBroker` is driven with `POMODORO_BROKER_NODE` pointing at a stub executable that records its invocation and re-execs `node` with the passed arguments; the check asserts the stub log names the override with `broker.mjs --idle` and that the spawned broker serves real IPC (check 3). Discriminator: with `spawn(process.execPath, ...)` restored, the stub is never invoked and the check fails (`ENOENT` on the stub log) — the old implementation provably ignores the override; the assertion does not depend on the test process's own runtime being Node.
- `verify` now validates the broker runtime (`verifyRuntimeNode`): `node` from PATH or the `POMODORO_BROKER_NODE` override must actually run, otherwise verification fails explicitly with guidance (check 18: broken override and unusable PATH both rejected).

**Check evidence**

- `node plugins/pomodoro-agent-status/scripts/check.mjs` → `pomodoro-agent-status check: 23 checks passed`; `node --check` on all plugin scripts clean.

**Criterion status updates**

- Criteria 2-8 remain passed; the on-demand-broker and broker-unavailable evidence is unchanged, and the broker now provably starts from a non-Node host via PATH or override.
- Criterion 1 remains pending human acceptance, and the previous human-TUI smoke result (bounded failure toast) is now explained and superseded by this fix: the coordinator must rerun the real-TUI smoke after a full quit/restart before criterion 1 can be accepted.

### 2026-09-06 Operational note (single coding executor, user-authorized clean reinstall)

Both Pomodoro installations were cleanly reinstalled from the current repository and both state directories were reset (full evidence in Ticket 05's operational comment). After an activation seam against the real device, exactly one current broker owns `/dev/cu.usbmodem11101` and publishes the activated Workspace's Idle, which should clear the stale blue Working dot. Criterion 1 remains pending human acceptance: the coordinator must rerun the real-TUI smoke (full OpenCode restart loads the reinstalled plugin, then `/pomodoro-select` autocomplete/dispatch/feedback) before it can be accepted.
