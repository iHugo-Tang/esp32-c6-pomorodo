# 05 — Remove the project-level plugin

**What to build:** Removing the now-superseded project-level plugin files from the Pomodoro repository so the user-level plugin is the only implementation loaded, with no double loading and no regression in device status behavior.

**Blocked by:** 02 — Add the global plugin wiring.

**Status:** ready-for-agent

- [ ] The project-level plugin directory and its check script are removed from the Pomodoro repository.
- [ ] A fresh OpenCode process in the Pomodoro repository loads exactly one plugin origin for OpenCode Status, and the device status, enable/disable, acknowledgement, reconnection, and flashing-release behaviors still work end to end.
- [ ] No remaining repository file imports or references the removed project-level plugin files.