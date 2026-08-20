# 01 — Route switch-row drags to the Settings list

**What to build:** Make every Settings row containing a switch remain visually fixed while a vertical drag that starts on the row background, label, or switch scrolls the Settings list. A drag must not change the switch state, while a tap on either the row or switch must continue to toggle it exactly once. Other Settings rows must retain their existing behavior.

**Blocked by:** None — can start immediately.

**Status:** ready-for-human

- [x] Auto-start breaks, Auto-start focus, and Sound rows do not scroll their own contents.
- [x] A vertical drag starting on a switch row is handled by the Settings list.
- [x] Dragging does not toggle a switch; tapping the row or switch toggles it exactly once.
- [x] Settings rows without switches retain their existing interaction behavior.
- [x] The firmware build passes without introducing a new UI-test framework or dependency.

## Comments

- 2026-08-20: Implemented in the shared switch-row constructor. Host tests pass 3/3 and the ESP-IDF 5.5 firmware build passes. Target-device interaction remains in Ticket 02.
