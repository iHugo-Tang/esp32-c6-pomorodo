# 01 — Refactor the Main app bar and add Menu navigation

**What to build:** Give Main a conventional app bar with a menu icon on the
left, the current Mode title centered, and battery status at the far right.
The menu opens a dedicated Menu page containing Statistics and Settings, with
predictable Back navigation through Menu to Main.

**Blocked by:** None — can start immediately.

**Status:** ready-for-human

- [x] Main shows a left menu icon button, a horizontally centered Mode title,
      and battery status aligned at the far right.
- [x] Activating the menu icon opens Menu, whose visible destinations are
      Statistics and Settings.
- [x] Statistics and Settings opened from Menu return to Menu; Back from Menu
      returns to Main.
- [x] Application navigation is covered through the public dispatch/view seam,
      and the complete host test suite and ESP-IDF firmware build pass.
- [ ] The updated firmware is exercised on the target device and the ticket
      records any remaining human-only visual verification.
