# 08 — Sync time from iPhone

**What to build:** Let a user explicitly synchronize accurate UTC and automatic daylight-saving information from an iPhone through a temporary local Wi-Fi session without accounts, internet access, or retained network credentials.

**Blocked by:** 05 — Set local time and handle RTC failure; 07 — Complete Settings and Numeric Stepper.

**Status:** ready-for-human

- [x] Sync starts a password-protected SoftAP with a random password, scannable QR code, and five-minute expiry.
- [x] The local page reads calibrated UTC, IANA time-zone name, and five years of UTC-offset transitions, but writes nothing until `Sync to Pomodoro` is confirmed.
- [x] Success writes UTC to the RTC, persists the offset schedule, resolves Pending Statistics, and returns to Main or Settings according to the entry source.
- [x] Timeout, validation failure, cancellation, or success shuts down Wi-Fi and deletes password, payload, and session state.
- [x] The device never joins a router, reaches the internet, stores Wi-Fi credentials, or leaves Wi-Fi enabled outside an explicit sync session.
- [x] The UI warns before the stored transition schedule expires and always retains manual setup as a fallback.

Implementation and host integration coverage are complete. The firmware builds and
boots on the ESP32-C6 target; `ready-for-human` records the remaining iPhone join,
QR scan, local-page confirmation, timeout, and cleanup observations on connected
hardware.
