# Date and time

## Local wall time

The battery-backed PCF85063 RTC stores device-local wall time. The product has no UTC offset, automatic time zone, daylight-saving rule, phone synchronization, SoftAP, QR code, Wi-Fi, Bluetooth, NTP, GPS, or internet time path.

A Statistics Day runs from local `00:00` to the next local `00:00`. Correcting local time preserves the remaining duration of an active timer and recomputes its absolute deadline. Future completed work uses the corrected local date. Work completed while the RTC is invalid remains Pending Statistics and is assigned to the Statistics Day in which valid time is restored.

## Time setup

An invalid RTC opens a non-skippable time editor before Main. The editor contains Year, Month, Day, Hour, and Minute rollers and a Confirm action. It uses 24-hour time and supports dates from 2020 through 2069.

The initial draft uses the firmware compilation date and `12:00`. Changing year or month immediately clamps an invalid day to the last day of the selected month. All rollers stop at their range boundaries and never wrap. A fast gesture applies its final settled value as one batch and causes one application redraw; ordinary rendering does not rebuild roller options.

First-time setup has no Back action. Settings contains one `Date & time` row showing the current local date and time. Opening it scrolls the shared content to the top so all five rollers and Confirm are visible. Back from the Settings editor discards its draft and returns to Settings. Confirm validates the draft, writes the RTC, rebases an active timer without changing its remaining duration, persists the result, and returns to the correct source.

The editor continues to process time ticks so temporary feedback expires normally. RTC state remains independent of application persistence: after an application-state reset, a valid RTC opens Main and an invalid RTC opens required setup. Format compatibility is owned by [Firmware boundaries](firmware-boundaries.md#persistence-formats).

## Removed behavior

Phone synchronization and all network and QR dependencies are absent, not merely hidden. Fixed UTC-offset and IANA time-zone designs are historical and are not current requirements.
