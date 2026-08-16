# 11 — Complete device release acceptance

**What to build:** Prove the assembled firmware satisfies the product specification on the target board and can be updated over USB without losing user state.

**Blocked by:** 06 — Show Today and the Trailing Seven Days; 08 — Sync time from iPhone; 09 — Connect sound, buttons, battery, and power; 10 — Protect the AMOLED and manage display lifecycle.

**Status:** ready-for-human

- [ ] A documented target-board run verifies display, touch, PCF85063 RTC, ES8311 audio, AXP2101 battery/power, side buttons, PWR, NVS, and temporary SoftAP behavior.
- [ ] Remaining time stays within ±1 second and Completion is emitted within one second of the RTC deadline under navigation, audio, rendering, and persistence load.
- [ ] The application runs within the board's 512 KB HP SRAM without PSRAM, full framebuffers, sustained allocation failures, or unacceptable interaction latency.
- [ ] Long-running and repeated reboot tests show no duplicate settlement, statistics loss, unexpected Wi-Fi use, or once-per-second Flash writes.
- [ ] A normal USB-C ESP-IDF firmware update preserves Settings, active timer state, Set Progress, and statistics; only an explicit full-chip erase removes data.
- [x] Native tests, firmware build, hardware acceptance steps, dependency versions, and known limitations are documented and reproducible.

The automated acceptance slice and non-destructive USB update have been run on the
target board. The remaining unchecked items require timed interaction, an iPhone,
power-source changes, audio observation, and extended reboot/runtime observation;
they remain assigned to the canonical `ready-for-human` role.
