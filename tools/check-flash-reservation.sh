#!/usr/bin/env bash
# Runnable check for the cooperative USB port reservation flow. Runs a copy of
# tools/flash.sh against a fake idf.py, verifying that the reservation exists
# while flashing, is owner-safe, reclaims stale owners, and is removed after
# success, flash failure, and the bounded-release timeout.
# Run with: tools/check-flash-reservation.sh
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CHECK_ROOT="$(mktemp -d)"
trap 'rm -rf "$CHECK_ROOT"' EXIT

mkdir -p "$CHECK_ROOT/.local/state/opencode/pomodoro-status" "$CHECK_ROOT/tools" "$CHECK_ROOT/fakebin"
cp "$ROOT/tools/flash.sh" "$CHECK_ROOT/tools/flash.sh"
chmod +x "$CHECK_ROOT/tools/flash.sh"

FAKE_PORT="$CHECK_ROOT/fake-port"
: > "$FAKE_PORT"

RESERVATION="$CHECK_ROOT/.local/state/opencode/pomodoro-status/flash-reservation"
SAW_RESERVATION="$CHECK_ROOT/reservation-seen"
FLASH_LOG="$CHECK_ROOT/flash.log"
IDF_ARGS_LOG="$CHECK_ROOT/idf-args.log"
IDF_PID_LOG="$CHECK_ROOT/idf-pid.log"

cat > "$CHECK_ROOT/fakebin/idf.py" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
printf '%s\n' "$*" > "${IDF_ARGS_LOG:-/dev/null}"
printf '%s\n' "$$" > "${FAKE_IDF_PID_LOG:-/dev/null}"
if [ -f "${FAKE_RESERVATION:-}" ]; then
  printf 'reservation-present\n' >> "${RESERVATION_CHECK:-/dev/null}"
  printf 'reservation-owner=%s\n' "$(cat "$FAKE_RESERVATION")" >> "${RESERVATION_CHECK:-/dev/null}"
else
  printf 'reservation-absent\n' >> "${RESERVATION_CHECK:-/dev/null}"
fi
if [ -n "${FAKE_REPLACEMENT_OWNER:-}" ]; then
  printf '%s\n' "$FAKE_REPLACEMENT_OWNER" > "$FAKE_RESERVATION"
fi
sleep "${FAKE_IDF_SLEEP:-0.2}"
exit "${FAKE_IDF_EXIT:-0}"
EOF
chmod +x "$CHECK_ROOT/fakebin/idf.py"

run_flash() {
  set +e
  PATH="$CHECK_ROOT/fakebin:$PATH" \
    FAKE_IDF_EXIT="${1:-0}" \
    FAKE_REPLACEMENT_OWNER="${2:-}" \
    FAKE_RESERVATION="$RESERVATION" \
    RESERVATION_CHECK="$SAW_RESERVATION" \
    IDF_ARGS_LOG="$IDF_ARGS_LOG" \
    FAKE_IDF_PID_LOG="$IDF_PID_LOG" \
    XDG_STATE_HOME="$CHECK_ROOT/.local/state" \
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
( exec 3<>"$FAKE_PORT"; exec sleep 30 ) &
HOLDER=$!
run_flash 0 && fail "held port should time out"
kill "$HOLDER" 2>/dev/null || true
wait "$HOLDER" 2>/dev/null || true
[ ! -e "$RESERVATION" ] || fail "reservation left behind after timeout"
grep -q "still held by another process" "$FLASH_LOG" \
  || fail "bounded-release timeout message missing"
pass "timeout: clear failure and cleanup"

# 4. Missing user state home: fail instead of reserving an unshared path.
set +e
env -u HOME -u XDG_STATE_HOME \
  PATH="$CHECK_ROOT/fakebin:$PATH" \
  "$CHECK_ROOT/tools/flash.sh" -p "$FAKE_PORT" > "$FLASH_LOG" 2>&1
status=$?
set -e
[ "$status" -ne 0 ] || fail "missing user state home should fail"
grep -q "cannot determine user state directory" "$FLASH_LOG" \
  || fail "missing user state home error missing"
pass "missing user state home: fails closed"

# 5. Live owner: a second flash cannot overwrite or delete its reservation.
printf '%s\n' "$$" > "$RESERVATION"
rm -f "$IDF_ARGS_LOG"
run_flash 0 && fail "live reservation owner should block a second flash"
[ "$(cat "$RESERVATION")" = "$$" ] || fail "live reservation owner was overwritten or removed"
[ ! -e "$IDF_ARGS_LOG" ] || fail "idf.py ran while another flash owned the reservation"
grep -q "reservation is already held" "$FLASH_LOG" \
  || fail "live reservation owner error missing"
rm -f "$RESERVATION"
pass "live owner: preserved and blocks second flash"

# 6. Stale owner: a dead PID is replaced by the current flash and cleaned up.
( exit 0 ) &
stale_pid=$!
wait "$stale_pid"
printf '%s\n' "$stale_pid" > "$RESERVATION"
rm -f "$SAW_RESERVATION"
run_flash 0 || {
  cat "$FLASH_LOG"
  fail "stale reservation should be reclaimed"
}
grep -q "reservation-owner=" "$SAW_RESERVATION" \
  || fail "replacement reservation owner was not observed"
! grep -q "reservation-owner=$stale_pid" "$SAW_RESERVATION" \
  || fail "stale reservation owner was not replaced"
[ ! -e "$RESERVATION" ] || fail "reclaimed reservation left behind"
pass "stale owner: reclaimed and cleaned up"

# 7. Owner-checked cleanup: never delete a reservation replaced during flash.
run_flash 0 "$$" || fail "flash with replacement owner exited non-zero"
[ "$(cat "$RESERVATION")" = "$$" ] || fail "replacement reservation owner was deleted"
rm -f "$RESERVATION"
pass "cleanup: preserves a replacement owner"

check_interruption() {
  local signal="$1" expected_status="$2" interrupted_flash_pid idf_pid status
  rm -f "$IDF_PID_LOG"
  set -m
  PATH="$CHECK_ROOT/fakebin:$PATH" \
    FAKE_IDF_SLEEP=30 \
    FAKE_RESERVATION="$RESERVATION" \
    RESERVATION_CHECK="$SAW_RESERVATION" \
    IDF_ARGS_LOG="$IDF_ARGS_LOG" \
    FAKE_IDF_PID_LOG="$IDF_PID_LOG" \
    XDG_STATE_HOME="$CHECK_ROOT/.local/state" \
    POMODORO_FLASH_RELEASE_TIMEOUT_S=2 \
    "$CHECK_ROOT/tools/flash.sh" -p "$FAKE_PORT" > "$FLASH_LOG" 2>&1 &
  interrupted_flash_pid=$!
  set +m
  for _ in {1..50}; do
    [ -s "$IDF_PID_LOG" ] && break
    sleep 0.05
  done
  [ -s "$IDF_PID_LOG" ] || fail "$signal interrupted flash did not start idf.py"
  idf_pid="$(cat "$IDF_PID_LOG")"
  kill -s "$signal" "$interrupted_flash_pid"
  set +e
  wait "$interrupted_flash_pid"
  status=$?
  set -e
  [ "$status" -eq "$expected_status" ] \
    || fail "$signal interrupted flash exited with $status instead of $expected_status"
  ! kill -0 "$idf_pid" 2>/dev/null || fail "idf.py survived $signal interrupted flash"
  [ ! -e "$RESERVATION" ] || fail "reservation left behind after $signal interruption"
  [ ! -e "${RESERVATION}.lock" ] || fail "reservation lock left behind after $signal interruption"
  pass "$signal interruption: child stopped and ownership cleaned"
}

# 8-9. Interruption: stop the flash process group, then remove marker and lock.
check_interruption TERM 143
check_interruption INT 130

echo "flash-reservation check: $checks checks passed"
