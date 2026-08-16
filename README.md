# Pomodoro

Offline Pomodoro firmware for the Waveshare ESP32-C6-Touch-AMOLED-2.16.

The repository currently contains the first executable slice of the product:

- a deterministic, host-testable timer and Pomodoro Set state machine;
- an idempotent, versioned 365-day statistics store with RTC-invalid pending
  results;
- a CRC-checked atomic persistence payload for settings, timer, and statistics;
- an ESP-IDF 5.5.2 application entry point;
- persistence and hardware service boundaries ready for NVS, RTC, display,
  touch, audio, and power implementations;
- default ESP32-C6 memory/flash configuration and a data-preserving partition
  table;
- native tests for completion, abort, pause/resume, set rollover, and reboot
  recovery rules.

The product specification lives in [`docs/POMODORO_APP_PROMPT.md`](docs/POMODORO_APP_PROMPT.md).

## Requirements

- ESP-IDF 5.5.2 for firmware builds
- CMake 3.16+ and a C11 compiler for native tests

## Run native tests

```sh
cmake -S test -B build/host
cmake --build build/host
ctest --test-dir build/host --output-on-failure
```

## Build firmware

From an ESP-IDF 5.5.2 shell:

```sh
idf.py set-target esp32c6
idf.py build
```

The first hardware integration should implement the interfaces declared in
`components/pomodoro_platform/include/pomodoro_platform.h`, then add the pinned
Waveshare-validated LVGL 9 and `esp_lvgl_port` versions. Those versions are not
guessed here because the product decision requires hardware validation.
