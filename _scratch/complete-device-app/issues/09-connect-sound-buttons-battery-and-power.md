# 09 — Connect sound, buttons, battery, and power

**What to build:** Make the board's physical controls and status hardware useful during normal operation, with completion audio and non-blocking status feedback that never disrupts timer correctness.

**Blocked by:** 03 — Complete the Main timer and Pomodoro Set; 07 — Complete Settings and Numeric Stepper.

**Status:** ready-for-human

- [x] `+/KEY` and `BOOT/-` adjust volume from 0–5 during runtime while preserving bootloader behavior during startup.
- [x] Sound off stores and restores the last non-zero volume, and side-button changes synchronize Settings and Main.
- [x] Focus completion plays two short rising tones and break completion plays one gentle tone; no other action produces sound.
- [x] Battery percentage, charging, USB power, and low-battery state are shown without cancelling an active timer.
- [x] RTC warning, one-shot Battery low warning, and volume feedback share one non-intercepting `StatusOverlay` with the specified fixed child order.
- [x] PWR short press sleeps or wakes the display and long press follows the board's power-off behavior.

## Comments

2026-08-16: Board adapters and application flows are implemented against the Waveshare pinout and ES8311/AXP2101 devices. Host tests cover volume persistence, warning lifetimes, timer continuity, and power actions; audio, physical keys, battery readings, and PMIC power behavior still require the connected-device checklist in `docs/HARDWARE_CONNECTION_TEST.md`.
