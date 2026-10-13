#!/usr/bin/env bash
# Usage: tools/flash.sh [-p PORT] [idf.py flash arguments...]
set -euo pipefail

for arg in "$@"; do
  case "$arg" in
    -p|--port|-p=*|--port=*) exec idf.py "$@" flash ;;
  esac
done
if [ -n "${POMODORO_PORT:-}" ]; then
  set -- -p "$POMODORO_PORT" "$@"
fi
exec idf.py "$@" flash
