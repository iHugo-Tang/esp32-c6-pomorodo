# 01 — Add the Main Sound button

**What to build:** Add the 48×48 Sound button to the bottom-right of Main so users can see and toggle the current sound state without leaving the timer. Reuse the existing sound toggle, persistence, and Volume overlay behavior, and update the related product documentation and application tests.

**Blocked by:** None — can start immediately.

**Status:** done

- [x] Main shows a 48×48 Sound button in the bottom-right with at least the existing 24px safe margin; it uses the speaker icon when volume is non-zero and the mute icon when volume is zero.
- [x] The button is visible in Ready, Running, Paused, and the Completion Transition, but is hidden outside Main and disabled during the three-second automatic-start countdown.
- [x] Activating the button reuses the existing sound toggle behavior, restores the last non-zero volume when enabling sound, persists the change, and keeps Settings and physical volume controls synchronized.
- [x] Activating the button shows the existing two-second Volume overlay with the current 0–5 level and does not alter Timer state or completion visual feedback.
- [x] A confirmation modal prevents the button from being activated through its blocking mask.
- [x] Application tests cover toggle state, last non-zero volume restoration, persistence across reboot, overlay lifetime, and unchanged Timer state; related product and prototype documentation describes the bottom-right button and touch-triggered Volume overlay.
