# 03 — Recover from orientation sensor failure

**What to build:** Keep the Pomodoro timer fully usable when automatic orientation sensing fails, explain the degraded behavior once, and resume automatic rotation after recovery.

**Blocked by:** 01 — Rotate the complete UI from stable placement.

**Status:** done

- [x] Fewer than three consecutive accelerometer read failures preserve the current UI Orientation without showing a warning.
- [x] Three consecutive failures preserve the last UI Orientation, disable further classification, and begin initialization retries every five seconds.
- [x] `Auto-rotate unavailable` appears as a non-blocking status message for three seconds and at most once per boot.
- [x] The warning does not pause a Timer, consume touch, obscure a blocking Dialog, or alter navigation.
- [x] A successful retry silently resumes acceleration sampling and stable-direction classification without a recovery message.
- [x] Startup continues in `0°` when initial accelerometer setup fails; core timer, RTC, persistence, display, touch, controls, and audio behavior remain available.
- [x] Host tests cover transient failures, the third-failure transition, one-time warning behavior, retry timing, preserved direction, and silent recovery.
- [x] The complete host test suite and ESP-IDF firmware build pass.
