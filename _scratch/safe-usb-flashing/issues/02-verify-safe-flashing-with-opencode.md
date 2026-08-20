# 02 — Verify safe flashing with a live OpenCode session

**What to build:** After fully restarting OpenCode with the updated plugin, verify on the target device that the supported flashing flow completes while OpenCode Status is active, without freezing or crashing OpenCode, and that status synchronization resumes afterward.

**Blocked by:** 01 — Release the USB port safely for flashing.

**Status:** ready-for-human

- [ ] OpenCode is fully quit and restarted before verification so the updated plugin is loaded.
- [ ] The device is connected and actively synchronizing OpenCode Status before flashing begins.
- [ ] Starting the supported flashing command causes the plugin to release the USB port without pausing or terminating OpenCode.
- [ ] OpenCode remains responsive for the entire build and flash operation.
- [ ] The latest firmware is written and verified successfully on the target device.
- [ ] The reservation is absent after the flashing command exits.
- [ ] The plugin reconnects automatically and OpenCode Status resumes without restarting OpenCode again.
- [ ] Repeating the flash does not leave stale reader processes, serial handles, or reservation state.
- [ ] Any observed failure and recovery evidence is recorded under this ticket's comments.
