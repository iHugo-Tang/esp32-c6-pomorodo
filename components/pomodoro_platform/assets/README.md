# Embedded audio assets

The user-provided `warning.mp3` and `rest.mp3` are stored here in the device's
native playback format: signed 16-bit little-endian PCM, 16 kHz, mono.
`pomodoro_board.c` expands each sample to stereo while streaming it to ES8311.

- `warning.pcm`: Agent Waiting notification, 0.504 seconds.
- `rest.pcm`: Focus and Break completion notification, 1.848 seconds.

The conversion command is:

```sh
ffmpeg -i INPUT.mp3 -ac 1 -ar 16000 -f s16le OUTPUT.pcm
```
