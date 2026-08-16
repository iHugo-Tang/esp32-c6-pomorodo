# 05 — Set local time and handle RTC failure

**What to build:** Let a user establish local date and time without a phone, and keep an active timer and completed work correct when the battery-backed RTC is invalid or is corrected during operation.

**Blocked by:** 04 — Recover a timer without data loss.

**Status:** ready-for-agent

- [ ] An invalid RTC at boot blocks Main and offers the non-skippable first-run time setup with a manual path.
- [ ] Manual setup validates date, 24-hour time, and fixed UTC offset, writes UTC to the RTC, and returns to the correct source.
- [ ] Correcting date or time preserves an active timer's remaining duration and recomputes its absolute deadline.
- [ ] RTC failure during a timer continues from a monotonic clock and completed Focus results enter Pending Statistics.
- [ ] Restoring valid time assigns Pending Statistics to the restored current Statistics Day without relabeling historical days.
- [ ] RTC failure and recovery behavior is covered through host application tests and a PCF85063 hardware test.
