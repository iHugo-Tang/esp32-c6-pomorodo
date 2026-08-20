# 05 — Set local time and handle RTC failure

**What to build:** Let a user establish local date and time without a phone, and keep an active timer and completed work correct when the battery-backed RTC is invalid or is corrected during operation.

**Blocked by:** 04 — Recover a timer without data loss.

**Status:** ready-for-human

- [x] An invalid RTC at boot blocks Main and offers the non-skippable first-run time setup with a manual path.
- [x] Manual setup validates local date and 24-hour time, writes them to the RTC, and returns to the correct source.
- [x] Correcting date or time preserves an active timer's remaining duration and recomputes its absolute deadline.
- [x] RTC failure during a timer continues from a monotonic clock and completed Focus results enter Pending Statistics.
- [x] Restoring valid time assigns Pending Statistics to the restored current Statistics Day without relabeling historical days.
- [ ] RTC failure and recovery behavior is covered through host application tests and a PCF85063 hardware test.

## Comments

2026-08-16: RTC setup, correction, monotonic fallback, Pending Statistics, and recovery-day attribution are implemented and covered by host application tests. The ESP-IDF firmware builds successfully. The PCF85063 portion remains in `docs/HARDWARE_CONNECTION_TEST.md` for connected-device execution.
