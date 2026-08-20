#!/usr/bin/env bash
# Runnable check for the cooperative USB port reservation flow. Runs a copy of
# tools/flash.sh against a fake idf.py, verifying that the reservation exists
# while flashing and is removed afterward on success, flash failure, and the
# bounded-release timeout.
# Run with: tools/check-flash-reservation.sh
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CHECK_ROOT="$(mktemp -d)"
trap 'rm -rf "$CHECK_ROOT"' EXIT

mkdir -p "$CHECK_ROOT/tools" "$CHECK_ROOT/.pomodoro" "$CHECK_ROOT/fakebin"
cp "$ROOT/tools/flash.sh" "$CHECK_ROOT/tools/flash.sh"
chmod +x "$CHECK_ROOT/tools/flash.sh"

FAKE_PORT="$CHECK_ROOT/fake-port"
: > "$FAKE_PORT"

RESERVATION="$CHECK_ROOT/.pomodoro/flash-reservation"
SAW_RESERVATION="$CHECK_ROOT/reservation-seen"
FLASH_LOG="$CHECK_ROOT/flash.log"
IDF_ARGS_LOG="$CHECK_ROOT/idf-args.log"

cat > "$CHECK_ROOT/fakebin/idf.py" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
printf '%s\n' "$*" > "${IDF_ARGS_LOG:-/dev/null}"
if [ -f "${FAKE_RESERVATION:-}" ]; then
  printf 'reservation-present\n' >> "${RESERVATION_CHECK:-/dev/null}"
else
  printf 'reservation-absent\n' >> "${RESERVATION_CHECK:-/dev/null}"
fi
sleep 0.2
exit "${FAKE_IDF_EXIT:-0}"
EOF
chmod +x "$CHECK_ROOT/fakebin/idf.py"

run_flash() {
  set +e
  PATH="$CHECK_ROOT/fakebin:$PATH" \
    FAKE_IDF_EXIT="${1:-0}" \
    FAKE_RESERVATION="$RESERVATION" \
    RESERVATION_CHECK="$SAW_RESERVATION" \
    IDF_ARGS_LOG="$IDF_ARGS_LOG" \
    POMODORO_FLASH_RELEASE_TIMEOUT_S=2 \
    "$CHECK_ROOT/tools/flash.sh" -p "$FAKE_PORT" > "$FLASH_LOG" 2>&1
  local status=$?
  set -e
  return $status
}

checks=0
pass() {
  checks=$((checks + 1))
  echo "ok $checks: $1"
}
fail() {
  echo "FAIL: $1"
  exit 1
}

# 1. Success: reservation is present while flashing and removed afterward.
run_flash 0 || fail "successful flash exited non-zero"
grep -Fqx -- "-p $FAKE_PORT flash" "$IDF_ARGS_LOG" \
  || fail "idf.py arguments were not ordered as '-p PORT flash'"
grep -q "reservation-present" "$SAW_RESERVATION" \
  || fail "reservation was not present during flashing"
[ ! -e "$RESERVATION" ] || fail "reservation left behind after success"
pass "success: reservation acquired and removed"

# 2. Flash failure: reservation removed and non-zero exit.
rm -f "$SAW_RESERVATION"
run_flash 1 && fail "failed flash exited zero"
grep -q "reservation-present" "$SAW_RESERVATION" \
  || fail "reservation was not present during failing flash"
[ ! -e "$RESERVATION" ] || fail "reservation left behind after flash failure"
pass "flash failure: reservation removed"

# 3. Timeout: a process keeps holding the port; clear failure and cleanup.
( exec 3<>"$FAKE_PORT"; sleep 30 ) &
HOLDER=$!
run_flash 0 && fail "held port should time out"
kill "$HOLDER" 2>/dev/null || true
wait "$HOLDER" 2>/dev/null || true
[ ! -e "$RESERVATION" ] || fail "reservation left behind after timeout"
grep -q "still held by another process" "$FLASH_LOG" \
  || fail "bounded-release timeout message missing"
pass "timeout: clear failure and cleanup"

echo "flash-reservation check: $checks checks passed"
