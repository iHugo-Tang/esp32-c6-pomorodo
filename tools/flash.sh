#!/usr/bin/env bash
# Supported firmware flashing flow. Reserves the user-level USB state directory
# marker so the OpenCode Status plugin releases its serial link before `idf.py
# flash` opens it, then always removes the reservation. Never signals the
# OpenCode process.
#
# Usage: tools/flash.sh [-p PORT] [idf.py flash arguments...]
#   Port resolution: explicit -p/--port, else $POMODORO_PORT, else the sole
#   /dev/cu.usbmodem* device. If none can be resolved the wait is skipped.
set -euo pipefail

if [ -n "${XDG_STATE_HOME:-}" ]; then
  POMODORO_STATE_DIR="${XDG_STATE_HOME}/opencode/pomodoro-status"
elif [ -n "${HOME:-}" ]; then
  POMODORO_STATE_DIR="${HOME}/.local/state/opencode/pomodoro-status"
else
  echo "error: cannot determine user state directory; set XDG_STATE_HOME or HOME" >&2
  exit 1
fi
RESERVATION="$POMODORO_STATE_DIR/flash-reservation"
RELEASE_TIMEOUT_S="${POMODORO_FLASH_RELEASE_TIMEOUT_S:-8}"

# Resolve an explicit port from the arguments without consuming them.
PORT=""
prev=""
for arg in "$@"; do
  if [ "$prev" = "-p" ] || [ "$prev" = "--port" ]; then
    PORT="$arg"
  fi
  case "$arg" in
    -p=*|--port=*) PORT="${arg#*=}" ;;
  esac
  prev="$arg"
done
if [ -z "$PORT" ] && [ -n "${POMODORO_PORT:-}" ]; then
  PORT="$POMODORO_PORT"
fi
if [ -z "$PORT" ]; then
  candidates="$(ls -d /dev/cu.usbmodem* 2>/dev/null || true)"
  if [ "$(printf '%s\n' "$candidates" | sed '/^$/d' | wc -l | tr -d ' ')" = "1" ]; then
    PORT="$candidates"
  fi
fi

wait_for_release() {
  local port="$1"
  local start="$SECONDS"
  while :; do
    if ! lsof "$port" >/dev/null 2>&1; then
      return 0
    fi
    if (( SECONDS - start >= RELEASE_TIMEOUT_S )); then
      echo "error: USB port $port is still held by another process after ${RELEASE_TIMEOUT_S}s" >&2
      echo "       close its owner (e.g. idf.py monitor) and retry." >&2
      return 1
    fi
    sleep 0.5
  done
}

cleanup() {
  rm -f "$RESERVATION"
}
trap cleanup EXIT INT TERM HUP

mkdir -p "$POMODORO_STATE_DIR"
# Reserve before the port is opened; the plugin observes this file.
printf '%s\n' "$$" > "$RESERVATION"

if [ -n "$PORT" ]; then
  echo "Waiting for OpenCode to release $PORT ..."
  wait_for_release "$PORT" || exit 1
fi

idf.py "$@" flash
