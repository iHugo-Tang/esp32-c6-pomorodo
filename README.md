# Pomodoro

Offline Pomodoro firmware for the Waveshare ESP32-C6-Touch-AMOLED-2.16.

![Pomodoro timer running on the ESP32 display](docs/esp32-screen.png)

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
- configurable 1–60-second Pomodoro and fixed three-second break auto-start
  transitions with Start now and Cancel auto-start controls;
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

## Flash firmware while Agent Status is running

The project USB port is shared between the Agent Status broker and firmware
flashing. Use the supported wrapper instead of calling `idf.py flash` directly:

1. Connect the device and confirm its serial port, such as
   `/dev/cu.usbmodem11101`.
2. From the repository root, load ESP-IDF and run the wrapper:

   ```sh
   . "$HOME/esp/esp-idf/export.sh"
   tools/flash.sh -p /dev/cu.usbmodem11101
   ```

3. Wait for `Hash of data verified` and `Hard resetting via RTS pin`. The
   wrapper then removes its reservation and Agent Status reconnects
   automatically.

The wrapper writes `pomodoro/agent-status/flash-reservation` in
`$XDG_STATE_HOME` (or `~/.local/state` if unset) (containing its PID) before
opening the port, waits up to `POMODORO_FLASH_RELEASE_TIMEOUT_S` (default 8 s)
for the Agent Status broker to release the serial link, runs `idf.py flash`,
and always removes the reservation on success, failure, interruption, or
timeout. It never sends stop or termination signals to the broker. The port can
also be given via `POMODORO_PORT` or auto-detected when exactly one
`/dev/cu.usbmodem*` exists.

If release times out, close another serial owner such as `idf.py monitor` and
retry the same wrapper command. Do not use `erase-flash`; it deletes saved
product state.

## Agent Status

`$pomodoro-select` starts the dependency-free broker on demand when no broker
socket is listening. That automatic instance exits and releases the serial
port after ten seconds without a valid Agent `report`. To keep a broker
running independently, start the foreground entry point:

```sh
node tools/pomodoro-agent-status-broker.mjs
```

The broker is the only Agent Status serial owner. It keeps selection and
revision under `$XDG_STATE_HOME/pomodoro/agent-status` (default
`~/.local/state/pomodoro/agent-status`). The foreground entry remains
running until interrupted; neither mode installs a LaunchAgent or PID file.

Before first using this Codex-only version, stop any broker from an older
development build and manually remove its obsolete state directory. The new
runtime deliberately does not inspect, migrate, or delete it:

```sh
rm -rf -- "${XDG_STATE_HOME:-$HOME/.local/state}/opencode/pomodoro-status"
```

### Install the Codex Workspace adapter

The source of truth is [`plugins/pomodoro-agent-status`](plugins/pomodoro-agent-status).
Create the personal marketplace entry with the Codex `plugin-creator` helper,
copy this directory to `~/plugins/pomodoro-agent-status`, then install it:

```sh
python3 ~/.codex/skills/.system/plugin-creator/scripts/create_basic_plugin.py \
  pomodoro-agent-status --with-marketplace
ditto plugins/pomodoro-agent-status "$HOME/plugins/pomodoro-agent-status"
codex plugin add pomodoro-agent-status@personal
```

Open a new thread, run `/hooks`, and review and trust the plugin hooks. Restart
Codex Desktop before Desktop acceptance. Run `$pomodoro-select` in a Codex
thread to select its Codex Workspace. Chats and subagents opened anywhere
inside the same Git repository share one Agent Source; non-Git directories are
grouped by their resolved working directory.

For updates, copy the repository directory again, run
`update_plugin_cachebuster.py` from the `plugin-creator` skill against the
installed copy, reinstall from the personal marketplace, and test in a new
thread. Do not edit the installed copy independently.

Codex hooks and their trust flow are documented in the
[official hooks documentation](https://learn.chatgpt.com/docs/hooks). The
selection command is a skill because
[custom prompts are deprecated](https://learn.chatgpt.com/docs/custom-prompts).

The target integration uses the interfaces declared in
`components/pomodoro_platform/include/pomodoro_platform.h` with exact,
Waveshare-validated LVGL 9, `esp_lvgl_port`, display, and touch component
versions recorded in `main/idf_component.yml` and `dependencies.lock`.
