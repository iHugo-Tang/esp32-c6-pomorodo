# 06 — Show Today and the Trailing Seven Days

**What to build:** Give the user a compact Statistics screen showing today's completed Pomodoros, Completed Focus Duration, Daily Goal progress, and the Trailing Seven Days, using the local Statistics Day rules.

**Blocked by:** 04 — Recover a timer without data loss; 05 — Set local time and handle RTC failure.

**Status:** ready-for-agent

- [ ] Statistics opened from Main or Settings returns to the same source and remains non-scrolling.
- [ ] Today, Focus time, and Goal values reflect only naturally completed Focus timers and allow percentages above 100% while capping visual fill.
- [ ] The seven bars run oldest to newest, label dates as `M/D`, highlight today, show counts, and scale to both Daily Goal and observed maximum.
- [ ] A Focus spanning midnight is assigned wholly to the Statistics Day on which it completes.
- [ ] The store retains 365 daily aggregates and removes the oldest when a newer 366th day is inserted.
- [ ] Invalid RTC hides the chart and offers date/time setup; the first Pomodoro reaching Daily Goal receives the one-time enhanced completion treatment.
