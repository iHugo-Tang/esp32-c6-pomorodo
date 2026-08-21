# 02 — Global plugin wiring with serial ownership

**What to build:** The user-level OpenCode plugin that wires the coordination core to OpenCode and the USB device: loads in every project, registers its instance, and makes only the selected process own the serial port, send aggregated state and three-second heartbeats, honor `ENABLED`, `DISABLED`, and `ACK`, poll the global flashing reservation, and reconnect silently. Non-selected processes keep aggregating events but never write to the port.

**Blocked by:** 01 — Add the pure coordination core.

**Status:** ready-for-agent

- [ ] Every OpenCode process loading the user-level plugin participates in coordination; the selected process alone opens the serial port and sends `OC1 STATE` lines with the shared global revision.
- [ ] Aggregation, priority (Error > Waiting > Working > Completed > Idle), revision stability across heartbeats, and ACK handling behave as before, now sourced from the shared global revision.
- [ ] `ENABLED` resumes writes, `DISABLED` stops them, missing or occupied ports reconnect silently with bounded retries, and the plugin never fails an OpenCode task.
- [ ] While the global flashing reservation is held, the plugin does not campaign or open the serial port; when released, election and connection resume.
- [ ] If the state directory is unusable, the plugin fails closed without opening the serial port, and recovers once it becomes available.
- [ ] With two OpenCode processes in different projects, the first automatically selected process drives the device; the second does not steal; killing the selected process falls back to the surviving one — verified end to end.
- [ ] The plugin introduces no npm dependency and sends no prompt, code, response, title, or error detail to the device.