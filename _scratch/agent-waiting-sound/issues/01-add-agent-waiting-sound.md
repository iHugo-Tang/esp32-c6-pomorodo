# 01 — Add Agent Waiting Sound

**What to build:** Add the default-on `Waiting sound` setting and play one distinct, best-effort short tone whenever the effective aggregate Agent Status enters or regains Waiting. Preserve timer-completion audio priority, keep the behavior global across pages, reset legacy development persistence according to the agreed version policy, and cover the behavior through public host-test seams.

**Blocked by:** None — can start immediately.

**Status:** ready-for-human

- [x] Settings shows an independently persisted, default-on `Waiting sound` switch in the `Agent` section immediately after `Agent status`; it remains visible and editable while Agent Status is off.
- [x] A transition from any effective non-Waiting state to Waiting requests exactly one approximately 740 Hz, 120 ms tone at the current non-zero volume.
- [x] First sync, re-enable resync, timeout recovery, and Error acknowledgement that reveal Waiting each request one tone.
- [x] Repeated heartbeats, same-color revisions, additional Waiting members while already yellow, setting enable, unmute, and volume increase do not replay a current Waiting state.
- [x] Waiting Sound works on every page and during Focus or Break without changing navigation, Timer, Set Progress, or Statistics.
- [x] Waiting Sound is suppressed when its switch is off, Agent Status is off, global volume is zero, or audio is busy.
- [x] A dropped or failed Waiting tone is not queued, retried, or surfaced as Audio Error.
- [x] Focus and Break completion audio remains higher priority and is never lost because of Waiting Sound.
- [x] No broker, Codex plugin, member aggregation, revision, `OC1`, Agent Status color, acknowledgement, timeout, wake, or selection behavior changes.
- [x] Settings and persisted-state versions are advanced; old development records reset Settings, Timer, Set Progress, and Statistics, while a new/default state enables Waiting Sound.
- [x] Application and platform tests cover the complete trigger matrix, switches and volume, all-page behavior, timer isolation, audio busy/failure, completion priority, persistence, and legacy reset.
- [x] `CONTEXT.md`, current product documentation, and hardware acceptance documentation consistently describe Waiting Sound and its settings behavior.
- [x] The complete native host test suite and ESP-IDF firmware build pass.

## Comments

- 2026-08-31: Verification — native host tests pass 6/6; ESP-IDF 5.5.5 firmware build passes with `pomodoro.bin` size `0xc20c0` and 75% of the smallest app partition free. Code-review findings were resolved by narrowing the board layer to a fixed audio-priority policy, covering zero volume, and protecting the worker-start boundary. Target-device audio and interaction acceptance remains in issue 02.
