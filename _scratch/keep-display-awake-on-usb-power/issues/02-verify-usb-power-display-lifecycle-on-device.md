# 02 — Verify USB Power display lifecycle on the target device

**What to build:** Verify on the target board that USB Power prevents automatic dimming and Display Off, restores a display affected by automatic power saving, and causes the existing battery-powered lifecycle to resume after USB is removed, without changing user-requested Display Off or other display behavior.

**Blocked by:** 01 — Keep the display awake on USB Power.

**Status:** ready-for-human

- [ ] USB Power keeps normal brightness in Ready, Paused, Running, and Completion Transition.
- [ ] USB Power prevents automatic dimming and automatic Display Off at the existing idle thresholds.
- [ ] Connecting USB Power restores normal brightness after automatic dimming.
- [ ] Connecting USB Power wakes the display after automatic Display Off.
- [ ] User-requested Display Off remains effective while USB Power is present.
- [ ] Removing USB Power restarts idle timing; battery-powered dimming and Display Off occur only at the existing thresholds afterward.
- [ ] Charging without USB Power does not disable the battery-powered display policy.
- [ ] Battery, Charging, and USB Power indicators remain accurate.
- [ ] AMOLED pixel shift, touch wake, completion wake, and timer continuity remain unchanged.
- [ ] The verification is repeated across the relevant timer states and USB insertion/removal cycles.
