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

1. Fully quit every OpenCode process (also verify with `pgrep -fl opencode`), then run `node plugins/pomodoro-agent-status/scripts/install-opencode.mjs install` and start OpenCode fresh so the new user-level plugins load.
2. In the TUI, type `/` and confirm exactly one `pomodoro-select` entry appears in slash autocomplete (no duplicate Markdown command of the same name).
3. Invoke it in a session with visible work; confirm a bounded success (or waiting) toast appears with a revision, and that no prompt is submitted, no Session message/Task is created, and no model request is made (session transcript and token usage must stay unchanged).
4. Repeat the invocation and confirm the feedback shows a new revision. Stop any on-demand broker (`pkill -f "broker.mjs --idle"`; a foreground broker stops with Ctrl+C) and invoke again to observe failure feedback that keeps the previous selection.
5. Record the observed results in this ticket's comments.

Physical-device acceptance (criteria 6-7): with the broker running against real hardware, observe Idle/Working/Waiting/Error/Completed from the selected OpenCode Workspace (drive a session through work, a permission prompt, a failing tool, and completion), confirm Pomodoro timers/statistics are untouched, switch between OpenCode and Codex selections on device, disconnect/reconnect USB and confirm same-Workspace recovery, confirm no second serial owner appears, and run `tools/flash.sh` to confirm the broker releases and restores the port.

**Remaining work / notes**

- The three pending criteria above are the only blockers to calling the shared specification complete; everything automatable has been implemented and checked.
