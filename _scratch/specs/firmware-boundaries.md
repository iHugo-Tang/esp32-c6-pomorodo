# Firmware boundaries

## Platform constraints

The target is Waveshare ESP32-C6-Touch-AMOLED-2.16 with a 480×480 SH8601 QSPI AMOLED, CST9217 touch controller, PCF85063 RTC, ES8311 audio, 16 MB Flash, 512 KB HP SRAM, and no PSRAM.

Firmware uses ESP-IDF `>=5.5.2,<5.6`, a specifically pinned LVGL 9 version, `esp_lvgl_port`, and pinned Waveshare-compatible drivers. Display rendering uses small partial buffers and never allocates a full RGB565 framebuffer or full-screen double buffers. Touch response and timer accuracy take priority over animation quality.

UI components, the Pomodoro domain state machine, and persistence remain layered. Pages do not read or write storage directly.

The application boundary accepts user actions and time, renders observable view state, and emits storage, display, audio, and power effects through injected platform operations. Platform failures return diagnosable results. A valid RTC boots to Focus Ready; an invalid RTC enters required time setup.

## Persistence formats

Settings and top-level persisted state carry explicit format versions. Unreleased development formats that predate the current Agent Status and Waiting Sound settings are treated as empty storage rather than producing Storage Error. Such a reset restores default Settings, an empty Timer and Set Progress, and empty Statistics; `Agent status` and `Waiting sound` default to on. The independent RTC is not erased by this application-state reset.

Current product state uses a dual-slot journal so Settings, Timer, Set Progress, and Statistics survive interrupted writes. The Timer record distinguishes unsettled from settled completion; recovery details live in [Pomodoro state and statistics](pomodoro-and-statistics.md#recovery).

Application firmware and user data use separate update partitions. A normal USB firmware update preserves Settings, active timer state, Set Progress, and Statistics; only an explicit full-chip erase clears them. The development-format reset policy does not weaken released-update guarantees. After the first released format, every supported firmware update provides a migration from the previous supported version.

## Application interface

The public Application interface contains only:

- `create`
- `dispatch`
- batch `adjust_manual_time`
- `handle_touch`
- `destroy`

`dispatch` is the main behavior boundary. Batch roller adjustment exists so one gesture can apply multiple steps with one final render. Touch remains a separate production adapter because it reports whether a wake-only contact was consumed. Destroy is the lifecycle pair for create.

Settings confirmation, manual-time confirmation, and numeric-editor operations remain private. Brightness preview, cancel restoration, confirmation, date/time confirmation, and persistence use user-reachable actions through dispatch. Manual-time confirmation is valid only in the time-setup phase. Tests drive the same action and page constraints as production, except that batch roller and touch adapters may be tested directly.

Do not add test-only public APIs, conditional-compilation seams, a command framework, handler table, general command objects, or direct opaque-state access for these flows.

## Device View growth

Device View may retain one flat internal context and its narrow public interface. File length, context-field count, function count, and LVGL-object count are not extraction thresholds.

A new Application page or full-screen phase starts with a same-file `create_<page>()` and `render_<page>()` pair. Overlays retain their own create and render helpers. Before structurally changing an existing page's controls, callbacks, state, or rendering branches, first give that page a same-file create/render pair. Copy, color, and single layout-value edits do not trigger this prefactor.

Evaluate page extraction when there is evidence of independent lifecycle or ownership: page-specific timers or animation, lazy creation or destruction, repeated hidden-state leakage, cross-page callback branching, allocation failure, repeated changes across at least three cross-cutting areas, persistent merge conflicts, or a genuine page-level test seam. Extraction must solve the ownership issue, preserve the public interface, and pass only minimal internal parameters; it must not expose the global context or LVGL objects.

Do not add speculative page structs, registries, factories, virtual dispatch, handler tables, fake LVGL, renderer frameworks, or line-count CI gates. Structural UI changes require native host tests and a full firmware build; changes affecting visuals, touch, gestures, orientation, display lifecycle, animation, or memory also require target-device verification.
