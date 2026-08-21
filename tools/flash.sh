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
RESERVATION_LOCK="${RESERVATION}.lock"
RELEASE_TIMEOUT_S="${POMODORO_FLASH_RELEASE_TIMEOUT_S:-8}"
RESERVATION_LOCK_OWNED=0
FLASH_PID=""
SIGNAL_NAME=""
SIGNAL_STATUS=0
CLEANING_UP=0

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
    exit_if_signaled
    if python3 -c 'import fcntl, os, sys; fd = os.open(sys.argv[1], os.O_RDWR | os.O_NONBLOCK); fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB); os.close(fd)' "$port" 2>/dev/null; then
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

acquire_reservation_lock() {
  local start="$SECONDS"
  while :; do
    if /usr/bin/shlock -p "$$" -f "$RESERVATION_LOCK"; then
      RESERVATION_LOCK_OWNED=1
      exit_if_signaled
      return 0
    fi
    exit_if_signaled
    if (( SECONDS - start >= RELEASE_TIMEOUT_S )); then
      echo "error: timed out acquiring flash reservation lock" >&2
      return 1
    fi
    sleep 0.1
  done
}

release_reservation_lock() {
  local owner status=0
  [ "$RESERVATION_LOCK_OWNED" -eq 1 ] || return 0
  owner="$(cat "$RESERVATION_LOCK" 2>/dev/null || true)"
  if [ "$owner" = "$$" ]; then
    rm -f "$RESERVATION_LOCK" || status=$?
  fi
  RESERVATION_LOCK_OWNED=0
  return "$status"
}

pid_is_alive() {
  case "${1:-}" in
    ''|*[!0-9]*) return 1 ;;
  esac
  kill -0 "$1" 2>/dev/null
}

acquire_reservation() {
  local owner tmp
  acquire_reservation_lock || return 1
  owner="$(cat "$RESERVATION" 2>/dev/null || true)"
  if pid_is_alive "$owner"; then
    release_reservation_lock
    echo "error: flash reservation is already held by process $owner" >&2
    return 1
  fi
  tmp="${RESERVATION}.$$.$RANDOM.tmp"
  if ! printf '%s\n' "$$" > "$tmp" || ! mv -f "$tmp" "$RESERVATION"; then
    rm -f "$tmp"
    release_reservation_lock
    return 1
  fi
  release_reservation_lock
}

cleanup() {
  local owner status=0 lock_status
  CLEANING_UP=1
  if [ "$RESERVATION_LOCK_OWNED" -eq 0 ]; then
    acquire_reservation_lock || return $?
  fi
  owner="$(cat "$RESERVATION" 2>/dev/null || true)"
  if [ "$owner" = "$$" ]; then
    rm -f "$RESERVATION" || status=$?
  fi
  release_reservation_lock || {
    lock_status=$?
    [ "$status" -ne 0 ] || status="$lock_status"
  }
  return "$status"
}

cleanup_on_exit() {
  local exit_status=$? cleanup_status=0
  trap - EXIT
  cleanup || cleanup_status=$?
  [ "$exit_status" -ne 0 ] && exit "$exit_status"
  exit "$cleanup_status"
}

record_signal() {
  local signal="$1" status="$2"
  SIGNAL_NAME="$signal"
  SIGNAL_STATUS="$status"
  forward_pending_signal
}

forward_pending_signal() {
  if [ -n "$FLASH_PID" ] && [ -n "$SIGNAL_NAME" ]; then
    kill -s "$SIGNAL_NAME" -- "-$FLASH_PID" 2>/dev/null || true
  fi
}

exit_if_signaled() {
  if [ "$SIGNAL_STATUS" -ne 0 ] && [ "$CLEANING_UP" -eq 0 ]; then
    exit "$SIGNAL_STATUS"
  fi
}

mkdir -p "$POMODORO_STATE_DIR"
trap cleanup_on_exit EXIT
trap 'record_signal INT 130' INT
trap 'record_signal TERM 143' TERM
trap 'record_signal HUP 129' HUP
# Reserve before the port is opened; the plugin observes this file.
acquire_reservation
exit_if_signaled

if [ -n "$PORT" ]; then
  echo "Waiting for OpenCode to release $PORT ..."
  wait_for_release "$PORT" || exit 1
fi
exit_if_signaled

set -m
idf.py "$@" flash &
FLASH_PID=$!
set +m
forward_pending_signal
if wait "$FLASH_PID"; then
  status=0
else
  status=$?
fi
FLASH_PID=""
exit_if_signaled
exit "$status"
