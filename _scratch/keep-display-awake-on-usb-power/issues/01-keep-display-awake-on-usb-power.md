# 01 — Keep the display awake on USB Power

**What to build:** When the device is receiving `USB Power`, keep the display at the user's normal brightness instead of applying automatic dimming or Display Off. Restore a display dimmed or turned off by automatic power saving after USB is connected, and restart idle timing when USB Power disappears. Include application regression coverage, the required domain and product documentation updates, and firmware build verification.

**Blocked by:** None — can start immediately.

**Status:** complete

- [x] `USB Power` is determined from the existing external-power status, independently of charging and USB serial connection state.
- [x] While USB Power is present, Running does not apply the 60% idle dimming rule.
- [x] While USB Power is present, Ready, Paused, and Completion Transition do not apply Idle brightness or automatic Display Off.
- [x] A display dimmed or turned off by automatic power saving wakes and restores the user's normal brightness on the next state update after USB Power appears.
- [x] User-requested Display Off remains effective while USB Power is present.
- [x] When USB Power disappears, idle timing starts from that transition and existing battery-powered dimming and Display Off thresholds resume.
- [x] USB insertion and removal do not alter timer state, remaining time, Pomodoro Set, statistics, or persistence.
- [x] AMOLED pixel shift and existing display lifecycle behavior remain intact.
- [x] Application-orchestrator tests cover USB Power, insertion recovery, removal timing, charging without USB Power, and user-requested Display Off.
- [x] The `USB Power` domain term and affected product and hardware documentation are updated.
- [x] The complete host test suite and ESP-IDF firmware build pass.

## Comments

- 2026-08-20: Verification — host tests 4/4 pass (`cmake --build build/host --parallel && ctest --test-dir build/host --output-on-failure`); ESP-IDF build passes (`idf.py build`, `pomodoro.bin` 0xb2330 bytes, 77% app partition free).
- 2026-08-20: Committed as `feat: keep display awake on USB power` (180f206). `CONTEXT.md` keeps only the `USB Power` definition; the lifecycle behavior lives in the spec and product/hardware documentation.
