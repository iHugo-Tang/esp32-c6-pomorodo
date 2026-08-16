# 09 — Connect sound, buttons, battery, and power

**What to build:** Make the board's physical controls and status hardware useful during normal operation, with completion audio and non-blocking status feedback that never disrupts timer correctness.

**Blocked by:** 03 — Complete the Main timer and Pomodoro Set; 07 — Complete Settings and Numeric Stepper.

**Status:** ready-for-agent

- [ ] `+/KEY` and `BOOT/-` adjust volume from 0–5 during runtime while preserving bootloader behavior during startup.
- [ ] Sound off stores and restores the last non-zero volume, and side-button changes synchronize Settings and Main.
- [ ] Focus completion plays two short rising tones and break completion plays one gentle tone; no other action produces sound.
- [ ] Battery percentage, charging, USB power, and low-battery state are shown without cancelling an active timer.
- [ ] RTC warning, one-shot Battery low warning, and volume feedback share one non-intercepting `StatusOverlay` with the specified fixed child order.
- [ ] PWR short press sleeps or wakes the display and long press follows the board's power-off behavior.
