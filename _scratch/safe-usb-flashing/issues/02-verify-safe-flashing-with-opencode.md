# 02 — Verify safe flashing with a live OpenCode session

**What to build:** After fully restarting OpenCode with the updated plugin, verify on the target device that the supported flashing flow completes while OpenCode Status is active, without freezing or crashing OpenCode, and that status synchronization resumes afterward.

**Blocked by:** 01 — Release the USB port safely for flashing.

**Status:** ready-for-human

- [x] OpenCode is fully quit and restarted before verification so the updated plugin is loaded.
- [x] The device is connected and actively synchronizing OpenCode Status before flashing begins.
- [x] Starting the supported flashing command causes the plugin to release the USB port without pausing or terminating OpenCode.
- [x] OpenCode remains responsive for the entire build and flash operation.
- [x] The latest firmware is written and verified successfully on the target device.
- [x] The reservation is absent after the flashing command exits.
- [x] The plugin reconnects automatically and OpenCode Status resumes without restarting OpenCode again.
- [x] Repeating the flash does not leave stale reader processes, serial handles, or reservation state.
- [x] Any observed failure and recovery evidence is recorded under this ticket's comments.
