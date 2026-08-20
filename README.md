# Pomodoro

Offline Pomodoro firmware for the Waveshare ESP32-C6-Touch-AMOLED-2.16.

The repository currently contains an executable device slice of the product:

- a deterministic, host-testable timer and Pomodoro Set state machine;
- an idempotent, versioned 365-day statistics store with RTC-invalid pending
  results;
- a CRC-checked atomic persistence payload for settings, timer, and statistics;
- a dual-slot NVS journal that keeps the previous state recoverable across a
  torn write;
- an ESP-IDF 5.5.x application entry point (5.5.2 or newer, below 5.6);
- a 480×480 LVGL Main timer with Focus, Short break, and Long break pages,
  pause/resume/reset controls, completion transitions, and Set Progress;
- non-wrapping touch gestures with confirmation before an active timer is
  discarded;
- three-second auto-start transitions with Start now and Cancel auto-start
  controls that preserve the configured automation settings;
- persistence and hardware service boundaries for NVS, RTC, display, touch,
  audio, and power implementations;
- manual local date and time setup, used whenever the RTC has no valid time,
  plus a single Settings `Date & time` entry;
- default ESP32-C6 memory/flash configuration and a data-preserving partition
  table;
- native tests for completion, abort, pause/resume, all four Pomodoro Set
  rounds, automation, and reboot recovery rules.

The product specification lives in [`docs/POMODORO_APP_PROMPT.md`](docs/POMODORO_APP_PROMPT.md).
The successful ESP32-C6 serial communication check is recorded in
[`docs/HARDWARE_CONNECTION_TEST.md`](docs/HARDWARE_CONNECTION_TEST.md).

## Requirements

- ESP-IDF `>=5.5.2,<5.6` for firmware builds
- CMake 3.16+ and a C11 compiler for native tests

## Run native tests

```sh
cmake -S test -B build/host
cmake --build build/host
ctest --test-dir build/host --output-on-failure
```

## Build firmware

From an ESP-IDF 5.5.x shell (5.5.2 or newer, below 5.6):

```sh
idf.py set-target esp32c6
idf.py build
```

## Flash firmware while OpenCode is running

The project USB port is shared between the OpenCode Status plugin and firmware
flashing. To flash without interrupting OpenCode, use the supported wrapper,
which reserves the port cooperatively:

```sh
tools/flash.sh -p /dev/cu.usbmodem11101
```

The wrapper writes `.pomodoro/flash-reservation` (containing its PID) before
opening the port, waits up to `POMODORO_FLASH_RELEASE_TIMEOUT_S` (default 8 s)
for the OpenCode Status plugin to release the serial link, runs `idf.py flash`,
and always removes the reservation on success, failure, interruption, or
timeout. It never sends stop or termination signals to OpenCode. The port can
also be given via `POMODORO_PORT` or auto-detected when exactly one
`/dev/cu.usbmodem*` exists.

After changing anything under `.opencode/plugins/`, fully quit and restart
OpenCode before verifying, so the updated plugin is loaded.

The target integration uses the interfaces declared in
`components/pomodoro_platform/include/pomodoro_platform.h` with exact,
Waveshare-validated LVGL 9, `esp_lvgl_port`, display, and touch component
versions recorded in `main/idf_component.yml` and `dependencies.lock`.
