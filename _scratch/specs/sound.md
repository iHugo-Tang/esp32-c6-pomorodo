# Sound and audio feedback

## Sound state and controls

Volume ranges from `0` through `5`. A positive value means sound is enabled; `0` means muted. Disabling sound remembers the last non-zero volume, and enabling it restores that value. Settings, the Main Sound button, and the physical volume buttons share and persist one state.

`KEY/+` and `BOOT/-` adjust the volume during normal runtime. Their boot-time hardware behavior, including BOOT download mode, is unchanged.

Main has a 48×48 px Sound button at the bottom right with at least 24 px safe margin. It uses the native speaker symbol at non-zero volume and the mute symbol at zero. It is visible in Ready, Running, Paused, and Completion Transition, hidden outside Main and in platform-error states, and blocked by modal confirmation. During the three-second automatic-start countdown it remains visible but disabled. A normal Completion Transition permits sound toggling.

Activating the Main button toggles sound and shows the Volume surface. The surface contains the sound icon, five level segments, and `0/5` through `5/5`. It appears immediately after a touch or physical volume action and disappears two seconds after the most recent volume action. A later adjustment restarts the deadline. This behavior applies on Main, Settings, and first-time date setup and never pauses or changes a timer.

## Completion audio

Natural Focus and break completion play the embedded `rest` sound at the current volume. Muting suppresses audio but never suppresses Completion Transition or display wake. No other Pomodoro action plays a sound.
