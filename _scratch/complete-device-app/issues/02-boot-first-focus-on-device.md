# 02 — Boot the first Focus on the device

**What to build:** Make the Waveshare ESP32-C6-Touch-AMOLED-2.16 boot into a usable Focus screen when its RTC is valid. The user can see the configured duration, touch Start, and watch an accurate countdown while the application uses real display, touch, clock, and NVS services.

**Blocked by:** 01 — Establish a testable application orchestrator.

**Status:** resolved

- [x] Exact Waveshare-validated LVGL 9, `esp_lvgl_port`, and board-support revisions are pinned for ESP-IDF `>=5.5.2,<5.6`.
- [x] The display uses small partial draw buffers and never allocates a full RGB565 framebuffer or full-screen double buffers.
- [x] With a valid RTC, the real device renders the English Focus Ready screen and starts only from the explicit Start button.
- [x] The ring and `MM:SS` update once per second from an absolute deadline with visible error no greater than one second.
- [x] Touch, RTC, display, and NVS initialization failures produce a diagnosable result instead of silently running the core only.

## Validation

- ESP-IDF 5.5.5 build passed within the supported `>=5.5.2,<5.6` range; host tests passed 2/2.
- The target identified SH8601 1.0.2 and CST9217, then reached Focus Ready with one 9,600-pixel (19,200-byte) LVGL partial buffer.
- A real Start touch was accepted. RTC/countdown samples were `1767255236/1500`, `1767255237/1499`, `1767255238/1498`, and `1767255239/1497`, demonstrating one-second absolute-deadline updates with zero-second visible error.
