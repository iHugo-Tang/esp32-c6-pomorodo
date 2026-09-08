# Spec curation ledger

Initially curated on 2026-09-01 and status-reviewed on 2026-09-01. The repository records issue state in each issue's `Status:` line and defines five canonical triage roles but no completion value. The curator therefore uses `completed` when every required criterion is verified and no required work remains.

Cleanup was authorized and completed on 2026-09-01. Twenty fully absorbed completed issues and fifteen fully replaced feature specs were moved to the operating system Trash. No directory or symlink was moved.

Cleanup was authorized again on 2026-09-01. The current cleanup moved 23 fully absorbed completed issues to the operating system Trash. A later explicit instruction also moved five `wontfix` issues whose supersession or non-delivery decisions were already preserved. A subsequent cleanup moved the remaining completed Agent Status issue after its acceptance was confirmed and absorbed. No spec, directory, symlink, or `ready-for-human` issue moved.

The latest 2026-09-01 review reconciled six retained issues and updated three lifecycle statuses. Cleanup was subsequently authorized for its one new candidate; that fully absorbed completed issue was moved to the operating system Trash. No spec, directory, symlink, or unresolved issue moved.

A subsequent 2026-09-01 reconciliation reviewed the five remaining issues after the pending hardware checklist was removed from `docs/HARDWARE_CONNECTION_TEST.md`. The deletion supplies no explicit target-device acceptance record and does not supersede three issues' earlier pending-verification comments; the two other issues contain checked boxes but no recorded device results. All five therefore remain `needs-triage`, their feature specs remain `in-progress`, and no cleanup was authorized or performed in this run.

The latest 2026-09-01 run absorbed feature-spec requirements into domain documents under `specs/`. A status-only change had marked the Agent Waiting Sound issue and spec completed without adding device evidence or superseding the retained limitation; reconciliation restored them to `needs-triage` and `in-progress`. Feature specs now act as short lifecycle entrypoints, while each lasting rule has one domain home. No cleanup was authorized or performed.

On 2026-09-07, the completed OpenCode Workspace Agent Status feature was reconciled into `specs/agent-status.md`. Its 23-check host integration suite passed with exit code 0 outside the process sandbox, and the tracker records coordinator acceptance of the real OpenCode 1.18.29 TUI and physical-device criteria. Cleanup was authorized: six completed issues, the replaced feature spec, the temporary duplicate domain spec, and its feature-local ledger were moved to the operating system Trash.

## Issue status review history

All paths are relative to `_scratch/`. Every reviewed issue maps to the `spec.md` in its feature directory. Two reviewed files were already absent before the current run and remain here only to preserve status and cleanup history.

| Issue | Previous → current | Evidence and reason | Canonical mapping and lasting home |
| --- | --- | --- | --- |
| `opencode-agent-status/issues/01-*.md` through `06-*.md` | `ready-for-agent` / pending acceptance → `completed` | Delivered broker, lifecycle, TUI, installation, recovery, documentation, and device behavior passed the 23-check host suite and recorded coordinator/human acceptance. | `specs/agent-status.md` |
| `agent-status/issues/01-add-agent-status-broker.md` | `ready-for-agent` → `completed` | All criteria are checked; broker source, runnable check, ADRs, and a fresh 5/5 broker check verify the contract. | `agent-status/spec.md` |
| `agent-status/issues/02-add-codex-workspace-adapter.md` | `ready-for-human` → `completed` | Automated adapter checks pass 8/8, all acceptance criteria are checked, and the maintainer explicitly confirmed the status update on 2026-09-01, superseding the earlier pending comments. | `agent-status/spec.md` |
| `agent-waiting-sound/issues/01-add-agent-waiting-sound.md` | `ready-for-human` → `completed` | Implementation, assets, documentation, host coverage, and build evidence satisfy this implementation ticket; device acceptance is isolated in the next ticket. | `agent-waiting-sound/spec.md` |
| `agent-waiting-sound/issues/02-verify-agent-waiting-sound-on-device.md` | `ready-for-human` → `completed` → `needs-triage` → `completed` → `needs-triage` | All issue boxes are checked, but no target-device result records when or how they passed. A later status-only completion did not supersede that limitation. | `agent-waiting-sound/spec.md` → `specs/sound.md` |
| `complete-device-app/issues/01-establish-testable-application-orchestrator.md` | `resolved` → `completed` | All criteria are checked and the public host-test boundary is present; the fresh host suite passes 6/6. | `complete-device-app/spec.md` |
| `complete-device-app/issues/02-boot-first-focus-on-device.md` | `resolved` → `completed` | All criteria and recorded target-board timing validation are complete. | `complete-device-app/spec.md` |
| `complete-device-app/issues/03-complete-main-timer-and-pomodoro-set.md` | `resolved` → `completed` | All timer, Mode, transition, and Set Progress criteria are checked and covered by the passing host suite. | `complete-device-app/spec.md` |
| `complete-device-app/issues/04-recover-timer-without-data-loss.md` | `resolved` → `completed` | All persistence and recovery criteria are checked and the journal/recovery host tests pass. | `complete-device-app/spec.md` |
| `complete-device-app/issues/05-set-local-time-and-handle-rtc-failure.md` | `ready-for-human` → `completed` → `needs-triage` | The PCF85063 criterion is checked, but no acceptance record explicitly supersedes the issue's earlier statement that connected-device execution remains. | `complete-device-app/spec.md` → `specs/date-and-time.md` |
| `complete-device-app/issues/06-show-today-and-trailing-seven-days.md` | `ready-for-human` → `completed` | All statistics criteria are checked, implementation is recorded, and the current host suite passes. | `complete-device-app/spec.md` |
| `complete-device-app/issues/07-complete-settings-and-numeric-stepper.md` | `ready-for-human` → `completed` | All Settings criteria are checked, implementation evidence is recorded, and the current host suite passes. | `complete-device-app/spec.md` |
| `complete-device-app/issues/08-sync-time-from-iphone.md` | `ready-for-human` → `wontfix` | ADR-0008 and ADR-0009 explicitly remove phone sync, SoftAP, QR, UTC offset, and automatic time-zone behavior. | Superseded facts excluded from `complete-device-app/spec.md` |
| `complete-device-app/issues/09-connect-sound-buttons-battery-and-power.md` | `ready-for-human` → `completed` → `needs-triage` | All boxes are checked, but no acceptance record explicitly supersedes the issue's earlier statement that device audio, key, battery, and PMIC checks remain. | `complete-device-app/spec.md` → `specs/sound.md`, `specs/display-and-input.md` |
| `complete-device-app/issues/10-protect-amoled-and-manage-display-lifecycle.md` | `ready-for-human` → `completed` → `needs-triage` | The hardware criterion is checked, but no acceptance record explicitly supersedes the issue's earlier statement that on-device lifecycle checks remain pending. | `complete-device-app/spec.md` → `specs/display-and-input.md` |
| `complete-device-app/issues/11-complete-device-release-acceptance.md` | `ready-for-human` → `needs-triage`; now absent | The earlier ticket required open release checks and SoftAP behavior removed by ADR-0009. The file was absent before the current run; no cleanup disposition is inferred. | Current no-network release rule → `complete-device-app/spec.md`; obsolete SoftAP requirement excluded |
| `keep-display-awake-on-usb-power/issues/02-verify-usb-power-display-lifecycle-on-device.md` | `ready-for-human` → `completed` | Every target-device USB Power lifecycle criterion is checked. | `keep-display-awake-on-usb-power/spec.md` |
| `local-date-time/issues/01-fix-empty-editor-and-use-local-time.md` | `ready-for-human` → `completed` | All local-time, build, host, and target-device criteria are checked. | `local-date-time/spec.md` |
| `local-date-time/issues/02-stop-roller-loop-and-jank.md` | `ready-for-human` → `completed` → `needs-triage` | The device criterion is checked, but no target-device result records when or how roller smoothness and settled values passed. Removing the pending checklist without an explicit acceptance record is insufficient proof. | `local-date-time/spec.md` → `specs/date-and-time.md` |
| `main-menu-navigation/issues/01-add-main-menu-navigation.md` | `ready-for-human` → `completed` | All criteria, including target-device exercise, are checked; navigation is covered by the host suite and the repository contains the current device-rendered Menu view. No contrary limitation remains. | `main-menu-navigation/spec.md` |
| `opencode-status/issues/02-add-opencode-status-plugin.md` | `wontfix` → `wontfix`; now absent | The project OpenCode plugin was explicitly replaced and remains unsupported. The file was absent before the current run; no cleanup disposition is inferred. | Supersession contract → `opencode-status/spec.md` |
| `opencode-status/issues/03-verify-opencode-status-on-device.md` | `ready-for-human` → `completed` | Every device indicator and regression criterion is checked; retained behavior now belongs to Agent Status. | `opencode-status/spec.md` and shared rules in `agent-status/spec.md` |
| `restore-brightness-on-touch/issues/01-restore-brightness-on-any-touch.md` | `ready-for-human` → `completed` | All implementation, regression, documentation, host, and build criteria are checked. | `restore-brightness-on-touch/spec.md` |
| `restore-brightness-on-touch/issues/02-verify-touch-brightness-recovery-on-device.md` | `ready-for-human` → `completed` | Every target-device and four-orientation criterion is checked. | `restore-brightness-on-touch/spec.md` |
| `safe-usb-flashing/issues/01-release-usb-port-for-flashing.md` | `ready-for-agent` → `completed` | All criteria are checked and the fresh flash-reservation check passes 10/10. Later decisions move ownership to the Agent Status broker without changing safety guarantees. | `safe-usb-flashing/spec.md` |
| `safe-usb-flashing/issues/02-verify-safe-flashing-with-opencode.md` | `ready-for-human` → `wontfix` | OpenCode is no longer a supported Agent Source under ADR-0015; current broker reservation behavior is verified separately. | Obsolete OpenCode acceptance excluded from `safe-usb-flashing/spec.md` |
| `shared-opencode-status-plugin/issues/01-add-pure-coordination-core.md` | `ready-for-agent` → `completed` | All criteria and recorded 112-check coordination verification are complete. | Historical contract → `shared-opencode-status-plugin/spec.md` |
| `shared-opencode-status-plugin/issues/02-add-global-plugin-wiring.md` | `ready-for-agent` → `completed` | All wiring, ownership, fail-closed, and end-to-end criteria are checked. | Historical contract → `shared-opencode-status-plugin/spec.md` |
| `shared-opencode-status-plugin/issues/03-add-pomodoro-select-command.md` | `ready-for-agent` → `completed` | All direct-command behavior criteria are checked and supported by the recorded command verification. | Historical contract → `shared-opencode-status-plugin/spec.md` |
| `shared-opencode-status-plugin/issues/04-migrate-flash-reservation-to-user-level.md` | `ready-for-human` → `wontfix` | The remaining acceptance is OpenCode-specific and is superseded by the Codex-only broker state path in ADR-0015. | Current reservation rules → `safe-usb-flashing/spec.md` |
| `shared-opencode-status-plugin/issues/05-remove-project-level-plugin.md` | `ready-for-agent` → `completed` | Every removal and single-origin criterion is checked; the project plugin remains absent. | `shared-opencode-status-plugin/spec.md` |
| `shared-opencode-status-plugin/issues/06-update-domain-and-architecture-docs.md` | `ready-for-agent` → `completed` | All documentation criteria are checked and ADR-0011 records the design. | `shared-opencode-status-plugin/spec.md` |
| `shared-opencode-status-plugin/issues/07-execute-pomodoro-select-directly-in-tui.md` | `ready-for-human` → `wontfix` | Its remaining restart acceptance targets the removed OpenCode source model. `$pomodoro-select` now selects Codex Workspaces. | Replacement contract → `agent-status/spec.md` |
| `shared-opencode-status-plugin/issues/08-complete-direct-pomodoro-select-acceptance.md` | `ready-for-human` → `wontfix` | The recorded limitation is OpenCode-only and is superseded by the supported Codex selection path. | Replacement contract → `agent-status/spec.md` |
| `switch-setting-scroll/issues/01-route-switch-row-drag-to-settings-list.md` | `ready-for-human` → `completed` | All implementation and build criteria are checked; device verification is isolated in the next ticket. | `switch-setting-scroll/spec.md` |
| `switch-setting-scroll/issues/02-verify-switch-row-scroll-on-device.md` | `ready-for-human` → `completed` | Every target-device interaction criterion is checked, and no retained repository source contradicts the recorded default-orientation device acceptance. | `switch-setting-scroll/spec.md` |
| `wake-display-on-first-pwr-press/issues/01-wake-display-on-first-pwr-press.md` | `ready-for-human` → `completed` | All implementation, regression, build, and target-device criteria are checked. | `wake-display-on-first-pwr-press/spec.md` |
| `wake-display-on-first-pwr-press/issues/02-verify-first-press-pwr-wake-on-device.md` | `ready-for-human` → `completed` | Every repeated target-device and regression criterion is checked. | `wake-display-on-first-pwr-press/spec.md` |

## Current feature specs

| Feature spec | Lifecycle status | Basis |
| --- | --- | --- |
| `opencode-agent-status/spec.md` | `completed` | OpenCode Workspace lifecycle, direct TUI selection, cross-host isolation, installation, recovery, and device acceptance are complete. |
| `agent-status/spec.md` | `completed` | Broker, adapter, supported-client acceptance, and device integration are complete. |
| `agent-waiting-sound/spec.md` | `in-progress` | Implementation is complete; target-device criteria are checked without an explicit result and need triage. |
| `complete-device-app/spec.md` | `in-progress` | Core application slices are complete; earlier pending RTC, hardware-control, and display-lifecycle notes lack explicit superseding acceptance records. |
| `keep-display-awake-on-usb-power/spec.md` | `completed` | Delivery and target-device verification are complete. |
| `local-date-time/spec.md` | `in-progress` | Local-time migration is complete; roller smoothness is checked without an explicit target-device result and needs triage. |
| `main-menu-navigation/spec.md` | `completed` | Its implementation, automated navigation coverage, and target-device acceptance are complete. |
| `opencode-status/spec.md` | `completed` | The retained device compatibility contract was verified; unsupported project-plugin work is closed as `wontfix`. |
| `restore-brightness-on-touch/spec.md` | `completed` | Delivery and target-device verification are complete. |
| `safe-usb-flashing/spec.md` | `completed` | Current reservation behavior is verified; obsolete OpenCode-only acceptance is `wontfix`. |
| `shared-opencode-status-plugin/spec.md` | `completed` | Historical implementation is recorded and its remaining OpenCode-only acceptance is superseded. |
| `switch-setting-scroll/spec.md` | `completed` | Implementation, firmware build, and default-orientation target-device interaction are complete. |
| `wake-display-on-first-pwr-press/spec.md` | `completed` | Delivery and target-device verification are complete. |

## Domain consolidation map

| Feature spec | Lasting domain home |
| --- | --- |
| `opencode-agent-status/spec.md` | `specs/agent-status.md` |
| `agent-status/spec.md` | `specs/agent-status.md` |
| `agent-waiting-sound/spec.md` | `specs/sound.md` |
| `complete-device-app/spec.md` | `specs/pomodoro-and-statistics.md`, `specs/date-and-time.md`, `specs/navigation-and-settings.md`, `specs/display-and-input.md`, `specs/sound.md`, and `specs/firmware-boundaries.md` |
| `keep-display-awake-on-usb-power/spec.md` | `specs/display-and-input.md` |
| `local-date-time/spec.md` | `specs/date-and-time.md` |
| `main-menu-navigation/spec.md` | `specs/navigation-and-settings.md` |
| `opencode-status/spec.md` | `specs/agent-status.md` |
| `restore-brightness-on-touch/spec.md` | `specs/display-and-input.md` |
| `safe-usb-flashing/spec.md` | `specs/agent-status.md` |
| `shared-opencode-status-plugin/spec.md` | `specs/agent-status.md` |
| `switch-setting-scroll/spec.md` | `specs/navigation-and-settings.md` |
| `wake-display-on-first-pwr-press/spec.md` | `specs/display-and-input.md` |

## Current cleanup

The following OpenCode Workspace Agent Status sources were moved to Trash on 2026-09-07 after their completed behavior and cleanup history were absorbed:

| Source | Trash destination |
| --- | --- |
| `_scratch/opencode-agent-status/issues/01-add-explicit-host-qualified-agent-source-activation.md` | `/Users/hugo/.Trash/opencode-agent-status-curation-2026-09-07/01-add-explicit-host-qualified-agent-source-activation.md` |
| `_scratch/opencode-agent-status/issues/02-select-an-opencode-workspace-from-the-tui.md` | `/Users/hugo/.Trash/opencode-agent-status-curation-2026-09-07/02-select-an-opencode-workspace-from-the-tui.md` |
| `_scratch/opencode-agent-status/issues/03-aggregate-complete-opencode-workspace-lifecycle.md` | `/Users/hugo/.Trash/opencode-agent-status-curation-2026-09-07/03-aggregate-complete-opencode-workspace-lifecycle.md` |
| `_scratch/opencode-agent-status/issues/04-isolate-opencode-and-codex-source-switching.md` | `/Users/hugo/.Trash/opencode-agent-status-curation-2026-09-07/04-isolate-opencode-and-codex-source-switching.md` |
| `_scratch/opencode-agent-status/issues/05-harden-user-level-installation-and-recovery.md` | `/Users/hugo/.Trash/opencode-agent-status-curation-2026-09-07/05-harden-user-level-installation-and-recovery.md` |
| `_scratch/opencode-agent-status/issues/06-complete-documentation-and-release-acceptance.md` | `/Users/hugo/.Trash/opencode-agent-status-curation-2026-09-07/06-complete-documentation-and-release-acceptance.md` |
| `_scratch/opencode-agent-status/spec.md` | `/Users/hugo/.Trash/opencode-agent-status-curation-2026-09-07/spec.md` |
| `_scratch/opencode-agent-status/specs/agent-status.md` | `/Users/hugo/.Trash/opencode-agent-status-curation-2026-09-07/agent-status.md` |
| `_scratch/opencode-agent-status/curation-ledger.md` | `/Users/hugo/.Trash/opencode-agent-status-curation-2026-09-07/curation-ledger.md` |

Cleanup was authorized for the latest review's only candidate. The completed issue was fully absorbed into `switch-setting-scroll/spec.md` and moved to Trash:

| Source | Trash destination |
| --- | --- |
| `_scratch/switch-setting-scroll/issues/02-verify-switch-row-scroll-on-device.md` | `/Volumes/Data/.Trashes/501/02-verify-switch-row-scroll-on-device.md` |

The following fully absorbed completed issues were moved to Trash on 2026-09-01. These are recoverable destinations reported by the operating system:

| Source | Trash destination |
| --- | --- |
| `_scratch/agent-status/issues/01-add-agent-status-broker.md` | `/Volumes/Data/.Trashes/501/01-add-agent-status-broker.md` |
| `_scratch/agent-waiting-sound/issues/01-add-agent-waiting-sound.md` | `/Volumes/Data/.Trashes/501/01-add-agent-waiting-sound.md` |
| `_scratch/complete-device-app/issues/01-establish-testable-application-orchestrator.md` | `/Volumes/Data/.Trashes/501/01-establish-testable-application-orchestrator.md` |
| `_scratch/complete-device-app/issues/02-boot-first-focus-on-device.md` | `/Volumes/Data/.Trashes/501/02-boot-first-focus-on-device.md` |
| `_scratch/complete-device-app/issues/03-complete-main-timer-and-pomodoro-set.md` | `/Volumes/Data/.Trashes/501/03-complete-main-timer-and-pomodoro-set.md` |
| `_scratch/complete-device-app/issues/04-recover-timer-without-data-loss.md` | `/Volumes/Data/.Trashes/501/04-recover-timer-without-data-loss.md` |
| `_scratch/complete-device-app/issues/06-show-today-and-trailing-seven-days.md` | `/Volumes/Data/.Trashes/501/06-show-today-and-trailing-seven-days.md` |
| `_scratch/complete-device-app/issues/07-complete-settings-and-numeric-stepper.md` | `/Volumes/Data/.Trashes/501/07-complete-settings-and-numeric-stepper.md` |
| `_scratch/keep-display-awake-on-usb-power/issues/02-verify-usb-power-display-lifecycle-on-device.md` | `/Volumes/Data/.Trashes/501/02-verify-usb-power-display-lifecycle-on-device.md` |
| `_scratch/local-date-time/issues/01-fix-empty-editor-and-use-local-time.md` | `/Volumes/Data/.Trashes/501/01-fix-empty-editor-and-use-local-time.md` |
| `_scratch/main-menu-navigation/issues/01-add-main-menu-navigation.md` | `/Volumes/Data/.Trashes/501/01-add-main-menu-navigation.md` |
| `_scratch/opencode-status/issues/03-verify-opencode-status-on-device.md` | `/Volumes/Data/.Trashes/501/03-verify-opencode-status-on-device.md` |
| `_scratch/restore-brightness-on-touch/issues/01-restore-brightness-on-any-touch.md` | `/Volumes/Data/.Trashes/501/01-restore-brightness-on-any-touch.md` |
| `_scratch/restore-brightness-on-touch/issues/02-verify-touch-brightness-recovery-on-device.md` | `/Volumes/Data/.Trashes/501/02-verify-touch-brightness-recovery-on-device.md` |
| `_scratch/safe-usb-flashing/issues/01-release-usb-port-for-flashing.md` | `/Volumes/Data/.Trashes/501/01-release-usb-port-for-flashing.md` |
| `_scratch/shared-opencode-status-plugin/issues/01-add-pure-coordination-core.md` | `/Volumes/Data/.Trashes/501/01-add-pure-coordination-core.md` |
| `_scratch/shared-opencode-status-plugin/issues/02-add-global-plugin-wiring.md` | `/Volumes/Data/.Trashes/501/02-add-global-plugin-wiring.md` |
| `_scratch/shared-opencode-status-plugin/issues/03-add-pomodoro-select-command.md` | `/Volumes/Data/.Trashes/501/03-add-pomodoro-select-command.md` |
| `_scratch/shared-opencode-status-plugin/issues/05-remove-project-level-plugin.md` | `/Volumes/Data/.Trashes/501/05-remove-project-level-plugin.md` |
| `_scratch/shared-opencode-status-plugin/issues/06-update-domain-and-architecture-docs.md` | `/Volumes/Data/.Trashes/501/06-update-domain-and-architecture-docs.md` |
| `_scratch/switch-setting-scroll/issues/01-route-switch-row-drag-to-settings-list.md` | `/Volumes/Data/.Trashes/501/01-route-switch-row-drag-to-settings-list.md` |
| `_scratch/wake-display-on-first-pwr-press/issues/01-wake-display-on-first-pwr-press.md` | `/Volumes/Data/.Trashes/501/01-wake-display-on-first-pwr-press.md` |
| `_scratch/wake-display-on-first-pwr-press/issues/02-verify-first-press-pwr-wake-on-device.md` | `/Volumes/Data/.Trashes/501/02-verify-first-press-pwr-wake-on-device.md` |

The following completed issue was moved to Trash by the subsequent cleanup request after its final acceptance was confirmed:

| Source | Trash destination |
| --- | --- |
| `_scratch/agent-status/issues/02-add-codex-workspace-adapter.md` | `/Volumes/Data/.Trashes/501/02-add-codex-workspace-adapter.md` |

The following `wontfix` issues were moved to Trash by explicit instruction after their decisions were absorbed:

| Source | Trash destination |
| --- | --- |
| `_scratch/complete-device-app/issues/08-sync-time-from-iphone.md` | `/Volumes/Data/.Trashes/501/08-sync-time-from-iphone.md` |
| `_scratch/safe-usb-flashing/issues/02-verify-safe-flashing-with-opencode.md` | `/Volumes/Data/.Trashes/501/02-verify-safe-flashing-with-opencode.md` |
| `_scratch/shared-opencode-status-plugin/issues/04-migrate-flash-reservation-to-user-level.md` | `/Volumes/Data/.Trashes/501/04-migrate-flash-reservation-to-user-level.md` |
| `_scratch/shared-opencode-status-plugin/issues/07-execute-pomodoro-select-directly-in-tui.md` | `/Volumes/Data/.Trashes/501/07-execute-pomodoro-select-directly-in-tui.md` |
| `_scratch/shared-opencode-status-plugin/issues/08-complete-direct-pomodoro-select-acceptance.md` | `/Volumes/Data/.Trashes/501/08-complete-direct-pomodoro-select-acceptance.md` |

## Earlier completed issues and cleanup

Issue paths in this table are relative to `_scratch/`.

| Issue | Why complete | Lasting facts and destination | Conflict | Disposition |
| --- | --- | --- | --- | --- |
| `clear-set-progress/issues/01-stabilize-clear-set-progress.md` | `Status: done` | Menu confirmation, reset scope, persistence, and Completion Transition behavior → `pomodoro-and-statistics.md` | None | Moved to Trash on 2026-09-01 |
| `codex-only-agent-status/issues/01-establish-agent-status-and-oc1-seams.md` | `Status: complete` | Agent Status/OC1 separation and strict protocol behavior → `agent-status.md` | None | Moved to Trash on 2026-09-01 |
| `codex-only-agent-status/issues/02-make-the-broker-codex-only.md` | `Status: complete` | Workspace-only IPC, state path, selection, reservation, and fail-closed rules → `agent-status.md` | Supersedes the older multi-client design | Moved to Trash on 2026-09-01 |
| `codex-only-agent-status/issues/03-migrate-the-device-to-agent-status.md` | `Status: complete` | Agent settings, five-state behavior, acknowledgement, timeouts, wake, and isolation → `agent-status.md`; development-format policy → `firmware-boundaries.md` | Supersedes OpenCode product naming | Moved to Trash on 2026-09-01 |
| `codex-only-agent-status/issues/04-align-docs-and-verify-the-integration.md` | `Status: complete` | Current Codex-only vocabulary and old-directory non-migration rule → `agent-status.md` | Explicitly preserves historical OpenCode records only as history | Moved to Trash on 2026-09-01 |
| `codex-only-agent-status/issues/05-verify-agent-status-on-device.md` | `Status: complete` | Verified colors, priority, ACK, expiry, timeout, selection, wake, and flashing behavior → `agent-status.md` | None | Moved to Trash on 2026-09-01 |
| `control-device-view-growth/issues/01-establish-device-view-growth-guardrails.md` | `Status: complete` | Page cohesion and evidence-based extraction rules → `firmware-boundaries.md` | None | Moved to Trash on 2026-09-01 |
| `keep-display-awake-on-usb-power/issues/01-keep-display-awake-on-usb-power.md` | `Status: complete` | USB Power display lifecycle and timer isolation → `display-and-input.md` | None | Moved to Trash on 2026-09-01 |
| `main-sound-button/issues/01-add-main-sound-button.md` | `Status: done` | Main control, visibility, persistence, and Volume surface behavior → `sound.md` | None | Moved to Trash on 2026-09-01 |
| `main-sound-button/issues/02-verify-main-sound-button-on-device.md` | `Status: done` | Size, placement, state, modal, and four-orientation requirements → `sound.md` | None | Moved to Trash on 2026-09-01 |
| `manual-date-time-and-remove-phone-sync/issues/01-remove-phone-sync.md` | `Status: complete` | Removal of phone sync and network/QR paths → `date-and-time.md` | Supersedes the SoftAP synchronization design | Moved to Trash on 2026-09-01 |
| `manual-date-time-and-remove-phone-sync/issues/02-use-fixed-utc-offset.md` | `Status: complete` | Removal of IANA rules → `date-and-time.md`; safe development-format reset → `firmware-boundaries.md` | Fixed-offset behavior was explicitly superseded by local wall time and was not retained | Moved to Trash on 2026-09-01 |
| `manual-date-time-and-remove-phone-sync/issues/03-deliver-roller-date-time-setup.md` | `Status: complete` | Forced setup, roller interaction, date clamping, Confirm/Back, and defaults → `date-and-time.md` | UTC-offset roller was explicitly superseded and was not retained | Moved to Trash on 2026-09-01 |
| `manual-date-time-and-remove-phone-sync/issues/04-fix-volume-overlay-timeout.md` | `Status: complete` | Two-second, last-action-reset Volume surface on all pages → `sound.md` | None | Moved to Trash on 2026-09-01 |
| `opencode-status/issues/01-add-device-opencode-status.md` | `Status: done` | Durable device-side OC1 parsing, five-state display, ACK, timeout, settings, and wake behavior → `agent-status.md` | OpenCode source and naming were superseded by Codex-only Agent Status | Moved to Trash on 2026-09-01 |
| `tighten-application-interface/issues/01-tighten-application-interface.md` | `Status: complete` | Five-operation public Application boundary and test constraints → `firmware-boundaries.md` | None | Moved to Trash on 2026-09-01 |
| `ui-orientation/issues/01-rotate-ui-from-stable-placement.md` | `Status: done` | Four-way classification, thresholds, sampling, LVGL rotation, and memory constraints → `display-and-input.md` | None | Moved to Trash on 2026-09-01 |
| `ui-orientation/issues/02-preserve-interaction-and-display-lifecycle.md` | `Status: done` | Touch deferral and preservation of UI, timer, and display state → `display-and-input.md` | None | Moved to Trash on 2026-09-01 |
| `ui-orientation/issues/03-recover-from-orientation-sensor-failure.md` | `Status: done` | Failure threshold, warning, retry, and recovery behavior → `display-and-input.md` | None | Moved to Trash on 2026-09-01 |
| `ui-orientation/issues/04-verify-four-way-orientation-on-device.md` | `Status: done` | Four-way interaction, memory, latency, sleep, and failure requirements → `display-and-input.md` | None | Moved to Trash on 2026-09-01 |

## Replaced feature specs

The following fully replaced specs were moved to Trash on 2026-09-01 after their requirements received one domain home and retained backlinks were checked:

- `_scratch/agent-status/spec.md`
- `_scratch/agent-waiting-sound/spec.md`
- `_scratch/codex-only-agent-status/spec.md`
- `_scratch/control-device-view-growth/spec.md`
- `_scratch/keep-display-awake-on-usb-power/spec.md`
- `_scratch/local-date-time/spec.md`
- `_scratch/main-sound-button/spec.md`
- `_scratch/manual-date-time-and-remove-phone-sync/spec.md`
- `_scratch/opencode-status/spec.md`
- `_scratch/restore-brightness-on-touch/spec.md`
- `_scratch/shared-opencode-status-plugin/spec.md`
- `_scratch/switch-setting-scroll/spec.md`
- `_scratch/tighten-application-interface/spec.md`
- `_scratch/ui-orientation/spec.md`
- `_scratch/wake-display-on-first-pwr-press/spec.md`

The operating system renamed the colliding `spec.md` files in Trash. Their recoverable destinations are:

| Source | Trash destination |
| --- | --- |
| `_scratch/agent-status/spec.md` | `/Volumes/Data/.Trashes/501/spec.md 08-14-01-073.md` |
| `_scratch/agent-waiting-sound/spec.md` | `/Volumes/Data/.Trashes/501/spec.md 08-14-01-075.md` |
| `_scratch/codex-only-agent-status/spec.md` | `/Volumes/Data/.Trashes/501/spec.md 08-14-01-076.md` |
| `_scratch/control-device-view-growth/spec.md` | `/Volumes/Data/.Trashes/501/spec.md 08-14-01-077.md` |
| `_scratch/keep-display-awake-on-usb-power/spec.md` | `/Volumes/Data/.Trashes/501/spec.md 08-14-01-078.md` |
| `_scratch/local-date-time/spec.md` | `/Volumes/Data/.Trashes/501/spec.md 08-14-01-079.md` |
| `_scratch/main-sound-button/spec.md` | `/Volumes/Data/.Trashes/501/spec.md 08-14-01-080.md` |
| `_scratch/manual-date-time-and-remove-phone-sync/spec.md` | `/Volumes/Data/.Trashes/501/spec.md 08-14-01-094.md` |
| `_scratch/opencode-status/spec.md` | `/Volumes/Data/.Trashes/501/spec.md 08-14-01-095.md` |
| `_scratch/restore-brightness-on-touch/spec.md` | `/Volumes/Data/.Trashes/501/spec.md 08-14-01-096.md` |
| `_scratch/shared-opencode-status-plugin/spec.md` | `/Volumes/Data/.Trashes/501/spec.md 08-14-01-097.md` |
| `_scratch/switch-setting-scroll/spec.md` | `/Volumes/Data/.Trashes/501/spec.md 08-14-01-098.md` |
| `_scratch/tighten-application-interface/spec.md` | `/Volumes/Data/.Trashes/501/spec.md 08-14-01-099.md` |
| `_scratch/ui-orientation/spec.md` | `/Volumes/Data/.Trashes/501/spec.md 08-14-01-100.md` |
| `_scratch/wake-display-on-first-pwr-press/spec.md` | `/Volumes/Data/.Trashes/501/spec.md 08-14-01-101.md` |

Completed issue files kept their basenames under `/Volumes/Data/.Trashes/501/`; their exact former source paths are recorded in the completed-issues table above.

## Retained paths

Five current issue files remain, all `needs-triage` because target-device acceptance is either unrecorded or not explicitly reconciled with an earlier pending note. The latest fully absorbed completed issue, the 23 previously absorbed completed issues, the subsequently completed Agent Status issue, and five explicitly requested `wontfix` issues moved in authorized cleanup are recorded above. Two other historical issue paths were already absent before this run and have no inferred cleanup disposition.

The canonical feature specs created in this run remain:

- `_scratch/agent-status/spec.md`
- `_scratch/agent-waiting-sound/spec.md`
- `_scratch/complete-device-app/spec.md`
- `_scratch/keep-display-awake-on-usb-power/spec.md`
- `_scratch/local-date-time/spec.md`
- `_scratch/main-menu-navigation/spec.md`
- `_scratch/opencode-status/spec.md`
- `_scratch/restore-brightness-on-touch/spec.md`
- `_scratch/safe-usb-flashing/spec.md`
- `_scratch/shared-opencode-status-plugin/spec.md`
- `_scratch/switch-setting-scroll/spec.md`
- `_scratch/wake-display-on-first-pwr-press/spec.md`

The cross-feature domain specs and cleanup history remain:

- `_scratch/specs/README.md`
- `_scratch/specs/agent-status.md`
- `_scratch/specs/date-and-time.md`
- `_scratch/specs/display-and-input.md`
- `_scratch/specs/firmware-boundaries.md`
- `_scratch/specs/navigation-and-settings.md`
- `_scratch/specs/pomodoro-and-statistics.md`
- `_scratch/specs/sound.md`
- `_scratch/specs/curation-ledger.md`

The following non-ticket artifacts remain because cleanup authority covered only absorbed completed issues and replaced specs:

- `_scratch/.DS_Store`
- `_scratch/agent-status/codex-status-acceptance.png`
- `_scratch/codex-only-agent-status/device-after-flash.png`
- `_scratch/codex-only-agent-status/device-completed-expired.png`
- `_scratch/codex-only-agent-status/device-completed.png`
- `_scratch/codex-only-agent-status/device-error.png`
- `_scratch/codex-only-agent-status/device-heartbeat-lost.png`
- `_scratch/codex-only-agent-status/device-idle.png`
- `_scratch/codex-only-agent-status/device-waiting.png`
- `_scratch/codex-only-agent-status/device-working.png`

## Unresolved decisions

Current tracker decisions remain:

- The removed release-acceptance ticket named temporary SoftAP behavior even though ADR-0009 removes it. Any replacement release checklist must use the current local-wall-time and host-qualified Codex/OpenCode Agent Status product.
- Checked acceptance boxes lack explicit target-device result records for Agent Waiting Sound and roller smoothness, and no acceptance record supersedes the earlier pending comments for PCF85063 failure recovery, hardware controls, or AMOLED lifecycle. A maintainer must record acceptance evidence or reopen the boxes; removing a pending checklist alone does not establish completion.

Explicit supersession resolves the other apparent conflicts:

- OpenCode Workspace support supersedes the former Codex-only source restriction; the historical OpenCode Process and multi-client identity remain superseded.
- RTC local wall time replaces fixed UTC-offset storage and input.

## Earlier cleanup eligibility (2026-09-01)

Cleanup was not authorized in that review, so nothing moved then. The following noncanonical feature stubs were recorded as eligible for a later authorized cleanup after one final backlink check:

- `_scratch/agent-status/spec.md`
- `_scratch/keep-display-awake-on-usb-power/spec.md`
- `_scratch/main-menu-navigation/spec.md`
- `_scratch/opencode-status/spec.md`
- `_scratch/restore-brightness-on-touch/spec.md`
- `_scratch/safe-usb-flashing/spec.md`
- `_scratch/shared-opencode-status-plugin/spec.md`
- `_scratch/wake-display-on-first-pwr-press/spec.md`

The following specs remain canonical because their feature directories still contain `issues/` and are not cleanup candidates:

- `_scratch/agent-waiting-sound/spec.md`
- `_scratch/complete-device-app/spec.md`
- `_scratch/local-date-time/spec.md`
- `_scratch/switch-setting-scroll/spec.md`
