# 01 — Stabilize Clear Set Progress

**What to build:** Add a reliable Clear Set Progress action to Menu. Menu continues to show Statistics and Settings and remains fully responsive; selecting Clear Set Progress opens the approved confirmation, and confirming resets Set Progress to `0/4` without changing today's statistics or the active timer. The result persists immediately and follows the agreed Completion Transition behaviour.

**Blocked by:** None — can start immediately.

**Status:** done

- [x] Menu shows Statistics, Settings, and Clear Set Progress without malformed content, touch starvation, or an LVGL task-watchdog event.
- [x] Selecting Clear Set Progress opens the approved confirmation; Cancel returns to Menu and Clear returns to Main.
- [x] Clearing changes only Set Progress, persists immediately, and schedules Short break when invoked during Completion Transition.
- [x] Host tests, firmware build, and physical-device acceptance checks pass without temporary diagnostic behaviour remaining in the release firmware.
