# Sound and audio feedback

## Sound state and controls

Volume ranges from `0` through `5`. A positive value means sound is enabled; `0` means muted. Disabling sound remembers the last non-zero volume, and enabling it restores that value. Settings, the Main Sound button, and the physical volume buttons share and persist one state.

`KEY/+` and `BOOT/-` adjust the volume during normal runtime. Their boot-time hardware behavior, including BOOT download mode, is unchanged.

Main has a 48×48 px Sound button at the bottom right with at least 24 px safe margin. It uses the native speaker symbol at non-zero volume and the mute symbol at zero. It is visible in Ready, Running, Paused, and Completion Transition, hidden outside Main and in platform-error states, and blocked by modal confirmation. During the three-second automatic-start countdown it remains visible but disabled. A normal Completion Transition permits sound toggling.

Activating the Main button toggles sound and shows the Volume surface. The surface contains the sound icon, five level segments, and `0/5` through `5/5`. It appears immediately after a touch or physical volume action and disappears two seconds after the most recent volume action. A later adjustment restarts the deadline. This behavior applies on Main, Settings, and first-time date setup and never pauses or changes a timer.

## Completion audio

Natural Focus and break completion play the embedded `rest` sound at the current volume. Muting suppresses audio but never suppresses Completion Transition or display wake. No other Pomodoro action plays a sound.

## Waiting Sound

Settings places a default-on, independently persisted `Waiting sound` switch immediately after `Agent status` in the `Agent` section. It remains visible and editable while Agent Status is off. It obeys global sound and the current volume.

Play the short `warning` sound once when the effective aggregate Agent Status:

- changes from Idle, Working, Completed, or Error to Waiting;
- first becomes valid as Waiting;
- returns as Waiting after Agent Status is re-enabled or a timed-out connection recovers; or
- reveals Waiting after Error is acknowledged.

Do not play for repeated heartbeats, a new revision that remains Waiting, another member joining an already Waiting aggregate, enabling the switch while already Waiting, unmuting, or raising volume. Waiting Sound is global across all pages and during Focus or Break. It never changes navigation, Timer state, Set Progress, Statistics, the Agent Status display, or display-wake behavior.

Waiting Sound is best effort. It is skipped when its switch is off, Agent Status is off, volume is zero, or audio is busy. A skipped or failed sound is not queued or retried and does not create Audio Error. Focus and Break completion audio has priority and must never be lost because of Waiting Sound; Waiting Sound may be dropped during contention.

The embedded `warning` and `rest` assets are converted before build to 16 kHz mono PCM and expanded to stereo for playback. Runtime MP3 decoding and a general notification or sound registry are not required.
