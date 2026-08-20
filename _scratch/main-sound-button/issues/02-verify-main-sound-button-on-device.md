# 02 — Verify the Main Sound button on the target device

**What to build:** Verify on the target device that the Main Sound button is positioned, readable, and touchable in normal use, and that its state and interaction rules remain correct across Main states, UI Orientations, automatic-start countdowns, modal confirmations, Settings, and physical volume controls.

**Blocked by:** 01 — Add the Main Sound button.

**Status:** done

- [x] The Sound button is 48×48 px, appears at the bottom-right within the safe margin, and responds reliably to touch.
- [x] Speaker and mute icons correctly represent enabled and muted sound in Ready, Running, Paused, and the Completion Transition.
- [x] Toggling from Main updates the state and Volume overlay, preserves Timer behavior, and synchronizes with Settings and the physical volume buttons.
- [x] The button is visibly disabled during the automatic-start countdown and cannot be activated through Reset or mode-switch confirmation modals.
- [x] The button remains correctly positioned and touchable in all four UI Orientations.
- [x] The documented target-device checks and the complete host test suite and firmware build pass before the ticket is marked complete.
