# 05: Harden user-level installation and recovery

**What to build:** Make the OpenCode integration reliable as one user-level installation across projects and restarts. Installation and compatibility checks must preserve the zero-model command contract, while runtime failures remain non-blocking and never create another device owner or silently switch sources.

**Blocked by:** 03: Aggregate complete OpenCode Workspace lifecycle.

**Status:** ready-for-agent

**Completion:** pending coordinator review

- [x] One idempotent user-level installation activates the server and TUI plugin surfaces for at least two different projects.
- [x] The Pomodoro project does not load a duplicate project-level copy of either plugin surface.
- [x] A complete OpenCode restart preserves the Selected Agent Source and allows the same Workspace to restore publication when members report again.
- [x] Installation or upgrade validation confirms the direct TUI slash API is available on the supported OpenCode version.
- [x] If direct slash registration is unavailable, validation fails explicitly and does not create a same-name Markdown command or any model-backed fallback.
- [x] Missing broker, broker restart, malformed IPC, protected-state failure, missing device, occupied port, and plugin disposal do not interrupt OpenCode.
- [x] Runtime failure feedback is bounded, contains no work content, and preserves the prior selection wherever activation did not complete.
- [x] State directory and socket permissions remain restricted, and corrupt or unsafe persisted broker state continues to fail closed without overwrite.
- [x] Automated installation and recovery checks use isolated temporary configuration and state and do not mutate the operator's active OpenCode configuration.

## Comments

### 2026-09-06 Implementation (single coding executor)

**Implementation summary**

- `plugins/pomodoro-agent-status/scripts/install-opencode.mjs` (new): user-level installer with `install`, `uninstall`, and `verify` commands (library functions exported for the host check). It copies the five authoritative script files (`opencode-server.mjs`, `opencode-tui.mjs`, `opencode-core.mjs`, `reporter.mjs`, `broker.mjs`) into `<configDir>/plugins/pomodoro-agent-status/` byte-identically and idempotently, registers the server surface in `opencode.json` `plugin` and the TUI surface in `tui.json` `plugin` by absolute path (skipping an already-matching entry), refuses to touch unparseable configuration files, refuses when `tui.json` `plugin_enabled` disables the plugin, and writes both configs atomically. `install`/`verify` validate the OpenCode version floor (`MINIMUM_OPENCODE_VERSION = "1.18.29"`, parsed from `opencode --version` by default or supplied for tests) and fail explicitly below it. `verify` also re-checks file identity, both config entries, the disable state, and — for a given `--project-dir` — that no project-level Pomodoro plugin copy exists under `.opencode/plugins/`. `uninstall` removes both entries and the installed directory.
- `opencode-tui.mjs`: the direct-API validation is now an explicit, tested init failure (`api.command.register`/`trigger` missing, or `api.ui.toast` missing) that registers no Markdown prompt command and no model-backed fallback.
- `check.mjs`: added `testInstallationAndRecovery()` (checks 11-12) and `readdir` import.

**Check evidence (exact commands and outcomes)**

- Red first: `node plugins/pomodoro-agent-status/scripts/check.mjs` failed with `ERR_MODULE_NOT_FOUND` for `install-opencode.mjs`; the direct-API-missing assertions failed before the explicit throw was added.
- After implementation: `node plugins/pomodoro-agent-status/scripts/check.mjs` → `pomodoro-agent-status check: 17 checks passed` (checks 11-12 new; checks 1-10 and 13-17 unchanged).
- CLI smoke against an isolated temp config with the real `opencode --version` (1.18.29): `install --config-dir <tmp>` → installed; `verify --config-dir <tmp> --project-dir <tmp/proj>` → verified; `uninstall --config-dir <tmp>` → removed directory and both config entries. The operator's real `~/.config/opencode` was never touched by any automated check; all check state lives under `mkdtemp` directories.

**Per-criterion results**

1. Passed. Two `install` runs into one isolated temp config produce exactly one plugin directory (byte-identical files), one `opencode.json` `plugin` entry, and one `tui.json` `plugin` entry; `verify` passes for two different temp project directories using that single user-level installation.
2. Passed. `verify` asserts no project-level Pomodoro copy for both temp projects and for the Pomodoro repository itself (which has no `.opencode/plugins/` Pomodoro entry); the installer never writes into a project directory.
3. Passed. Broker restart with persisted `{selected, selectedHost, revision}` state preserves the host-qualified selection (check 12), and the same Workspace reporting again after restart restores publication with a new revision (asserted `> firstRevision`); plugin re-initialization was exercised across checks 8, 9, and 12 (fresh plugin instances report and resume publication).
4. Passed. `install` and `verify` validate the version floor by parsing the supplied or freshly executed `opencode --version`; the CLI smoke used the real installed 1.18.29. The definitive direct-API check additionally runs at TUI plugin startup (criterion 5).
5. Passed. Loading the real TUI entry with a fake API lacking `api.command` rejects with an explicit error naming the direct registration API, and an API lacking `ui.toast` also rejects; no command is registered and no Markdown/model fallback exists anywhere in the TUI module.
6. Passed. Missing broker: server plugin lifecycle events (created/busy/user message) process without throwing while the broker is absent (client-touch counter stays empty, fail-open); selection then fails bounded when the broker cannot start. Broker restart: check 12. Malformed IPC: check 8 (impostor socket). Protected-state failure: corrupt `agent-coordination.json` makes `runBroker` fail closed and the failed activation leaves the file untouched. Missing device: broker with no resolvable port — selection succeeds and publication keeps resolving (serial retries). Occupied port: the serial path is covered by check 10's fake port (open/close/reservation) and the broker's bounded retry; a hard serial open failure only affects publication, never IPC. Plugin disposal: checks 9 and 12 dispose plugin instances without interrupting the host checks.
7. Passed. Failure feedback is a bounded category toast/exception ("broker unavailable" / "broker rejected the request" / "status source unavailable"), never contains work content or error details (check 8 regex), and the failed activation preserved the prior selection on disk (check 8: persisted file byte-identical; check 12: corrupt state not overwritten).
8. Passed. `stat` assertions: state directory `0700`, broker socket `0600`, persisted state file `0600`; corrupt state fails closed and is not overwritten (twice-asserted).
9. Passed. Every installation/recovery check runs against `mkdtemp` config and state directories and explicit `XDG_STATE_HOME`; no check constructs the real operator config path, and the installer is only exercised with an explicit `--config-dir` in this execution.

**Remaining work / notes**

- The real "fully quit and restart OpenCode, then use `/pomodoro-select` in the shipped TUI" acceptance remains a human step (Ticket 06); these checks cover the installer, plugin surfaces, and broker against controlled fakes.
- `uninstall` leaves an emptied `opencode.json`/`tui.json` behind if they contained no other configuration; it never deletes user files it did not create.

### 2026-09-06 Self-review fixes (single coding executor)

Two self-review findings were fixed:

1. **(Low) Duplicate registration from relative-path entries.** `entryMatches()` only recognized absolute and `file://` entries, so a manually added relative `plugin` entry (resolved by OpenCode relative to the config file) would be re-registered as an extra absolute entry, loading the plugin twice. `entryMatches()` now resolves relative entries against the config file's directory; a probe confirmed that an existing relative `tui.json` entry is recognized and `install` appends nothing.
2. **(Test hygiene) Tautological assertions.** `testInstallationAndRecovery()` contained a `failed = { ok: false }` constant assertion and a pass-either-way `activationFailure` check; both were removed and replaced with a strict assertion that the restarted broker still holds the same Selected Agent Source.

Evidence: `node plugins/pomodoro-agent-status/scripts/check.mjs` → 17 checks passed; CTest host suite unchanged.
