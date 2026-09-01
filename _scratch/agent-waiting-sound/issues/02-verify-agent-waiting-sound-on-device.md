# 02 — Verify Agent Waiting Sound on the target device

**What to verify:** Validate on the target device that Waiting Sound is distinct, audible exactly once at the agreed state boundaries, correctly controlled by Agent Status, Waiting sound, Sound, and volume settings, and never compromises timer completion audio or other device behavior.

**Blocked by:** 01 — Add Agent Waiting Sound.

**Status:** ready-for-human

- [ ] Settings shows `Waiting sound` after `Agent status`, defaults to on after the format reset, persists changes across reboot, and remains editable while Agent Status is off.
- [ ] Idle/Working/Completed/Error to Waiting produces one approximately 740 Hz, 120 ms tone that is audibly distinct from Focus and Break completion tones.
- [ ] Initial sync, Agent Status re-enable, connection timeout recovery, and Error acknowledgement that reveal Waiting each produce one tone.
- [ ] Repeated heartbeats, same-color revisions, sustained Waiting, switch enable, unmute, and volume increase do not replay the tone.
- [ ] The tone is heard from Main, Settings, Statistics, numeric editors, and confirmation dialogs without changing the current page.
- [ ] The tone works during Focus and Break without affecting remaining time, completion, Set Progress, Statistics, or display wake behavior.
- [ ] Waiting sound off, Agent Status off, and global mute each suppress the tone; non-zero volume levels produce the expected relative loudness.
- [ ] Audio contention drops Waiting Sound when necessary, while Focus and Break completion tones remain present and the application does not enter Audio Error.
- [ ] No additional overlay, text, animation, control, or Agent Status protocol/display change appears.
- [ ] The setting and sound behavior remain correct in all four UI Orientations.
- [ ] The documented persistence reset, complete native host test suite, ESP-IDF firmware build, and target-board checks pass before the ticket is marked complete.
