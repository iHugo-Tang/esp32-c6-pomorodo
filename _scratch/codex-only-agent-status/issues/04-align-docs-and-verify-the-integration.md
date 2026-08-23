# 04 — Align current docs and verify the integration

**What to build:** Make every current product and operator document describe the Codex-only Agent Status system and prove the complete firmware-to-broker integration builds and passes its automated checks, while preserving historical records as history.

**Blocked by:** 02 — Make the broker Codex-only; 03 — Migrate the device to Agent Status.

**Status:** complete

- [x] Current README, product prompt, prototype description, hardware guidance, and flashing instructions use Agent Status vocabulary and the new state directory.
- [x] Current Agent Status documentation describes Workspace-only identity and no longer promises an OpenCode adapter or multi-client IPC.
- [x] Historical ADR-0010, ADR-0011, completed issues, and superseded specs retain their original OpenCode language.
- [x] ADR-0015 remains the authority for Codex-only Agent Sources, while ADR-0012 clearly identifies the portion it supersedes.
- [x] Non-historical production code, tests, current specs, and operator documentation contain no OpenCode feature naming; legitimate OC1 protocol naming remains.
- [x] The manual prerequisite to stop the old broker and delete the old host state directory is documented; runtime code performs no automatic cleanup.
- [x] All native host tests, plugin checks, broker checks, and flash-reservation checks pass.
- [x] A complete ESP-IDF firmware build succeeds with the renamed modules and updated component declarations.
- [x] No new protocol version, host abstraction, client registry, network dependency, or compatibility layer is introduced.

## Comments

- 2026-08-23: Updated README, product prompt, prototype description, hardware guidance, and flash instructions for Codex-only Agent Status and `$XDG_STATE_HOME/pomodoro/agent-status`. README records the manual old-directory deletion command; runtime performs no migration or cleanup. Native host tests (5/5), plugin checks (8/8), broker checks (5/5), flash-reservation checks (10/10), and the complete ESP-IDF build pass.
