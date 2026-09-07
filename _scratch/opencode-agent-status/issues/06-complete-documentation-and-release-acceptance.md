# 06: Complete documentation and release acceptance

**What to build:** Finish the OpenCode Agent Status release with current product and operator documentation, complete automated regression evidence, a real OpenCode TUI smoke test, and physical-device validation of status display, source switching, USB recovery, and flashing safety.

**Blocked by:** 04: Isolate OpenCode and Codex source switching; 05: Harden user-level installation and recovery.

**Status:** ready-for-agent

**Completion:** pending human acceptance (real TUI smoke test and physical-device validation)

- [x] Product documentation describes OpenCode Workspace as a supported Agent Source and removes the superseded Codex-only behavior statement.
- [x] Operator guidance covers user-level installation, full OpenCode restart after configuration changes, `/pomodoro-select`, success and failure feedback, compatibility checks, and uninstall or disable behavior.
- [x] Documentation states that TUI is the only supported first-release selection frontend and that Desktop, Web, non-interactive CLI, and npm publication remain out of scope.
- [x] The complete host integration check and all existing Codex Agent Status regressions pass.
- [ ] A real OpenCode 1.18.29 TUI smoke test proves slash discovery, direct dispatch, visible feedback, and absence of a submitted prompt, Session message, Task, or model request.
- [ ] Physical hardware shows Idle, Working, Waiting, Error, and Completed from the selected OpenCode Workspace without changing Pomodoro state or statistics.
- [ ] Physical hardware confirms OpenCode/Codex source switching, disconnect and same-Workspace recovery, single serial ownership, and flash reservation release and restoration.
- [x] Any behavior not provable by automated host checks is recorded explicitly with its hardware or human validation result.

## Comments

### 2026-09-06 Documentation and release-acceptance pass (single coding executor)

**Documentation changes**

- `docs/product/agent-status.md`: the source model paragraph now states that both Codex and OpenCode integrations are delivered, with `$pomodoro-select` for Codex and the direct TUI `/pomodoro-select` for OpenCode; the "Codex-only delivery" and "OpenCode server/TUI not delivered" statements are removed; a new "OpenCode 插件安装与卸载" section documents the installer commands, idempotency, full-restart requirement, 1.18.29 minimum version, explicit direct-API validation with no Markdown/model fallback, the three bounded feedback outcomes, and disable-vs-uninstall guidance; a TUI-only scope statement excludes OpenCode Desktop, Web, non-interactive CLI, and npm publication from the first release; the historical OpenCode Process paragraph now distinguishes the delivered broker-based integration.
- `_scratch/specs/agent-status.md`: the "delivered" statements are updated (both hosts delivered); a new paragraph pins the `/pomodoro-select` command contract (no prompt/Session message/Task/model request, no synthetic cycle, bounded toast, explicit failure without fallback, TUI-only first release); the lifecycle-adapter paragraph documents the OpenCode mapping including Question events and fail-open containment; the "OpenCode compatibility" section is replaced by "OpenCode compatibility and installation" documenting the installer, version floor, plugin registration locations, restart requirement, project-duplicate check, uninstall, and disable path.
- `README.md`: the Agent Status section now covers both hosts, splits Codex and OpenCode operator guidance, and documents the installer commands, restart requirement, `verify`/`--project-dir`, the zero-model `/pomodoro-select` contract, feedback outcomes, TUI-only scope, the deprecated-API risk with explicit failure (no fallback), and the foreground broker command. The directory table describes the plugin as covering both hosts.
- Feature documents created before implementation and required by this release are committed with this work: `CONTEXT.md` (Agent Source generalization plus the OpenCode Workspace term), `docs/adr/0015-...` supersession marker, `docs/adr/README.md` index entries, `docs/adr/0016-support-opencode-workspaces-as-agent-sources.md` (already accepted), and the confirmed planning handoff `docs/planning/opencode-agent-status.md`. Unrelated pre-existing edits in `AGENTS.md`, `docs/agents/domain.md`, and the Codex `pomodoro-select` skill were preserved and left unstaged.

**Check evidence (exact commands and outcomes)**

- `node plugins/pomodoro-agent-status/scripts/check.mjs` → `pomodoro-agent-status check: 17 checks passed` (the complete host integration check: Codex reducer/hook/reporter, broker activation/isolation/expiry/persistence, real plugin-entry selection, full lifecycle with child processes, cross-host isolation with serial recording and flash reservation, installation and recovery).
- `cmake -S test -B build/host` → configured; `cmake --build build/host --parallel` → built; `ctest --test-dir build/host --output-on-failure` → `100% tests passed out of 6` (all existing Codex/firmware-side Agent Status regressions pass; firmware behavior and `OC1` semantics are unchanged, so no firmware rebuild was performed).
- `node --check` on all five plugin scripts → clean.

**Per-criterion results**

1. Passed. `docs/product/agent-status.md` (with `CONTEXT.md`, the ADR-0015 supersession, and the ADR index) describes the OpenCode Workspace as a delivered Agent Source and removes the superseded Codex-only delivery statements.
2. Passed. Operator guidance (product doc "OpenCode 插件安装与卸载" + README OpenCode subsection) covers user-level install, full restart, `/pomodoro-select`, the three feedback outcomes, `verify` compatibility checks, and uninstall/disable.
3. Passed. Both docs state TUI-only first-release selection and that Desktop, Web, non-interactive CLI, and npm publication are out of scope.
4. Passed. 17-check host integration suite and all 6 CTest host suites pass (commands and outcomes above).
5. Pending human acceptance. A fake-TUI host check cannot prove slash discovery, direct dispatch, Toast rendering, or absence of a submitted Session message in the shipped OpenCode 1.18.29 TUI, and verifying it requires fully quitting every OpenCode process and reloading the active user configuration, which this execution must not do. Exact human steps are recorded below.
6. Pending human acceptance. Physical-device evidence is required; host checks and firmware builds cannot prove device display, wake, or statistics behavior.
7. Pending human acceptance. Physical-device evidence is required for USB exclusivity, disconnect recovery, and flash reservation on real hardware.
8. Passed. Every automated limitation is recorded: this file and the parent spec state that fake-TUI checks and host suites do not prove real-TUI or physical-device behavior; the on-demand double-revision interpretation is recorded in Tickets 02/05 context and the parent spec; plugin-disposal semantics rely on the broker lease (Ticket 03 note).

**Human acceptance instructions (pending)**

Real TUI smoke test (criterion 5):

1. Fully quit every OpenCode process (verify with `pgrep -fl opencode`), then run `node plugins/pomodoro-agent-status/scripts/install-opencode.mjs install` and start OpenCode fresh so the new user-level plugins load.
2. In the TUI, type `/` and confirm exactly one `pomodoro-select` entry appears in slash autocomplete (no duplicate Markdown command of the same name).
3. Invoke it in a session with visible work; confirm a bounded success (or waiting, when the broker had to start) toast appears with a revision, and that no prompt is submitted, no Session message/Task is created, and no model request is made (session transcript and token usage must stay unchanged).
4. Repeat the invocation and confirm the feedback shows a new revision. Then quit OpenCode fully again, start it once more, and invoke again to confirm the selection and publication recover with exactly one new revision. An induced real-TUI failure case is not required and no destructive mutation of the live setup is suggested; with the implemented activation, stopping the broker and re-invoking restarts it on demand and yields waiting/success feedback rather than failure.
5. If a broker must be stopped for any step, do not use broad `pkill -f` patterns; find the specific process (`pgrep -f "broker.mjs --idle"` for on-demand brokers, or run a foreground broker and stop it with Ctrl+C) and `kill <recorded pid>`.
6. Record the observed results in this ticket's comments.

Physical-device acceptance (criteria 6-7): with the broker running against real hardware, observe Idle/Working/Waiting/Error/Completed from the selected OpenCode Workspace (drive a session through work, a permission prompt, a failing tool, and completion), confirm Pomodoro timers/statistics are untouched, switch between OpenCode and Codex selections on device, disconnect/reconnect USB and confirm same-Workspace recovery, confirm no second serial owner appears, and run `tools/flash.sh` to confirm the broker releases and restores the port.

**Remaining work / notes**

- The three pending criteria above are the only blockers to calling the shared specification complete; everything automatable has been implemented and checked.

### 2026-09-06 Coordinator review round 1 fixes (single coding executor)

**Gaps fixed**

1. **(Documentation truth) Direct-API validation wording.** Product doc, README, and the parent spec previously implied the installer itself would fail when the direct slash API is unavailable. Corrected everywhere: installer validation covers the version floor and file/registration integrity only; the direct slash API is validated when the TUI plugin initializes in the real OpenCode process after a full restart, and that startup fails explicitly without any Markdown/model fallback.
2. **(Documentation truth) Smoke-test step 4.** The previous instruction claimed that stopping the broker and re-invoking yields failure feedback; with the implemented on-demand activation it actually restarts the broker and yields waiting/success. Step 4 now verifies repeat selection, full quit/restart recovery, and explains why an induced failure case is optional (with a safe optional procedure); step 5 forbids broad `pkill -f` patterns and requires a recorded PID (`pgrep -f "broker.mjs --idle"`) or a foreground broker.
3. **(Supporting implementation changes verified this round)** Event attribution is shape-aware (message events use `info.sessionID`), the reducer accepts the installed `permission.updated`/`permission.replied` events and Question-tool detection with documented `*.v2.*` forward-compat aliases, activation costs exactly one revision in every state, a failed selection keeps the previous activation's keep-alive alive, session Workspace moves are atomic and serialized, and the installer's `verify` resolves relative entries against the config directory. These are evidenced in Tickets 02, 03, and 05 comments and by the 19-check host suite.

**Per-criterion results (this round)**

1-4, 8. Passed, evidence unchanged or strengthened (criterion 8's recording now also covers the direct-API validation boundary and the one-revision activation contract).
5-7. Still pending human acceptance with the corrected instructions above.

**Check evidence**

- `node plugins/pomodoro-agent-status/scripts/check.mjs` → `pomodoro-agent-status check: 19 checks passed`; CMake configure/build and CTest → 6/6; `git diff --check` clean.

### 2026-09-06 Coordinator review round 2 fixes (single coding executor)

1. **(Safety) The optional failure procedure was removed.** The previous smoke instructions suggested revoking state-directory write permission in the live user setup — a destructive mutation that is unnecessary because a real failure case is not required by criterion 5. The instructions now contain only safe restart/reselection steps and recorded-PID process handling (`pgrep -f "broker.mjs --idle"` → `kill <pid>`, or a foreground broker stopped with Ctrl+C); broad `pkill -f` remains forbidden.
2. **(Documentation truth) Delivered vs accepted.** The product doc, curated spec, README, and parent spec now state explicitly that "delivered/implemented" means implemented and covered by the automated host checks, while the real OpenCode TUI smoke test and all physical-device criteria remain pending human execution; no doc claims validation that did not occur. The parent spec carries a dedicated "Delivery truth" note.
3. **(Supporting changes verified this round)** Atomic broker `activate` operation (member refresh + selection in one serialized step, exactly one revision per accepted invocation including concurrent ones) replaced the non-atomic select-first flow; installer duplicate detection covers both canonical project directories and JSONC/malformed configurations are refused before any mutation. Evidenced in Tickets 02 and 05 comments and by the 21-check host suite.

**Per-criterion results (this round)**

1-4, 8. Passed, evidence unchanged or strengthened.
5-7. Still pending human acceptance with the corrected, non-destructive instructions above.

**Check evidence**

- `node plugins/pomodoro-agent-status/scripts/check.mjs` → `pomodoro-agent-status check: 21 checks passed`; CMake configure/build and CTest → 6/6; `git diff --check` clean.

### 2026-09-06 Real-TUI round fix (single coding executor)

The coordinator's real OpenCode 1.18.29 TUI repro exposed a bug the fake-API acceptance could not see: inside the compiled TUI, `ensureBroker()` spawned `process.execPath` (the OpenCode/Bun binary), so no broker could ever start and selection always failed with bounded "broker unavailable" feedback. Fixed by spawning an explicit Node runtime (`POMODORO_BROKER_NODE` override, else `node` from PATH; Node hosts keep their own executable), with `verify` validating that runtime. Product doc, README, and the curated spec now document the broker child's explicit Node runtime, the override, and the explicit-failure behavior. This is recorded truthfully as a fake-API acceptance gap: host-runtime differences only surface in the real TUI. Criteria 5-7 remain pending human acceptance; the coordinator must rerun the real-TUI smoke (criterion 5, and with it Ticket 02 criterion 1) after a full quit/restart now that the broker can start from the TUI.

### 2026-09-06 Operational run note (single coding executor, user-authorized one-off)

A clean reinstall of both hosts and both Pomodoro state directories was performed at the user's request (details and per-criterion evidence in Ticket 05's operational comment). Server/serial-side evidence is now concrete: no legacy broker exists, the legacy state directory is removed, and exactly one current broker owns `/dev/cu.usbmodem11101`, publishing the activated Workspace's Idle. This does not change the acceptance boundaries: criteria 5-7 remain pending human acceptance — full Codex Desktop/OpenCode restarts, `/hooks` re-trust (hook hashes changed with the reinstall), `$pomodoro-select` and `/pomodoro-select` runs, source-switching observation, and the visual confirmation that the blue Working dot clears after idle without Pomodoro state changes.
