# 06 — Show Today and the Trailing Seven Days

**What to build:** Give the user a compact Statistics screen showing today's completed Pomodoros, Completed Focus Duration, Daily Goal progress, and the Trailing Seven Days, using the local Statistics Day rules.

**Blocked by:** 04 — Recover a timer without data loss; 05 — Set local time and handle RTC failure.

**Status:** ready-for-human

- [x] Statistics opened from Main or Settings returns to the same source and remains non-scrolling.
- [x] Today, Focus time, and Goal values reflect only naturally completed Focus timers and allow percentages above 100% while capping visual fill.
- [x] The seven bars run oldest to newest, label dates as `M/D`, highlight today, show counts, and scale to both Daily Goal and observed maximum.
- [x] A Focus spanning midnight is assigned wholly to the Statistics Day on which it completes.
- [x] The store retains 365 daily aggregates and removes the oldest when a newer 366th day is inserted.
- [x] Invalid RTC hides the chart and offers date/time setup; the first Pomodoro reaching Daily Goal receives the one-time enhanced completion treatment.

## Comments

2026-08-16: Statistics projection, navigation-source restoration, midnight attribution, 365-day eviction, invalid-RTC empty state, and one-time Daily Goal completion feedback are implemented and covered by host tests.
