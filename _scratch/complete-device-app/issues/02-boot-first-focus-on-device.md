# 02 — Boot the first Focus on the device

**What to build:** Make the Waveshare ESP32-C6-Touch-AMOLED-2.16 boot into a usable Focus screen when its RTC is valid. The user can see the configured duration, touch Start, and watch an accurate countdown while the application uses real display, touch, clock, and NVS services.

**Blocked by:** 01 — Establish a testable application orchestrator.

**Status:** ready-for-agent

- [ ] Exact Waveshare-validated LVGL 9, `esp_lvgl_port`, and board-support revisions are pinned for ESP-IDF `>=5.5.2,<5.6`.
- [ ] The display uses small partial draw buffers and never allocates a full RGB565 framebuffer or full-screen double buffers.
- [ ] With a valid RTC, the real device renders the English Focus Ready screen and starts only from the explicit Start button.
- [ ] The ring and `MM:SS` update once per second from an absolute deadline with visible error no greater than one second.
- [ ] Touch, RTC, display, and NVS initialization failures produce a diagnosable result instead of silently running the core only.
