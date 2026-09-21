#!/bin/bash
#
# End-to-end OTA test.
#
#   tools/ota/test-ota.sh <device_id> <firmware_url> [broker_host]
#
# Example:
#   tools/ota/serve.py &                       # serves build/smart_home.bin
#   tools/ota/test-ota.sh shnode-01 http://192.168.2.1:8070/smart_home.bin
#
# What it proves, and only this: the device fetched the image over HTTP,
# wrote it to the inactive slot, rebooted into it, and came back reporting a
# *different* firmware version. The version is the evidence -- a device that
# merely reconnects proves nothing, so the script refuses to call it a pass
# unless the version actually changed.

set -u

DEVICE_ID="${1:-}"
FIRMWARE_URL="${2:-}"
BROKER="${3:-$(ip -4 -o addr show scope global 2>/dev/null | awk '{print $4}' | cut -d/ -f1 | grep -E '^192\.168\.' | head -1)}"

if [ -z "$DEVICE_ID" ] || [ -z "$FIRMWARE_URL" ]; then
    echo "usage: $0 <device_id> <firmware_url> [broker_host]"
    exit 1
fi

BASE="smart_home/devices/$DEVICE_ID"
PASS=0; FAIL=0
ok()   { printf '  \033[0;32mPASS\033[0m  %s\n' "$1"; PASS=$((PASS + 1)); }
bad()  { printf '  \033[0;31mFAIL\033[0m  %s\n' "$1"; FAIL=$((FAIL + 1)); }
warn() { printf '  \033[0;33mWARN\033[0m  %s\n' "$1"; }
info() { printf '  \033[0;34m----\033[0m  %s\n' "$1"; }

retained() { timeout 5 mosquitto_sub -h "$BROKER" -t "$1" -C 1 -W 3 2>/dev/null; }
jget()     { python3 -c "import json,sys; print(json.load(sys.stdin).get('$1',''))" 2>/dev/null; }

echo "OTA test  ·  device $DEVICE_ID  ·  broker $BROKER"
echo "          image  $FIRMWARE_URL"
echo

# --- preflight ---------------------------------------------------------------
echo "1. Preflight"

if ! mosquitto_pub -h "$BROKER" -t "_smarthome_test/ping" -m ping 2>/dev/null; then
    bad "broker $BROKER:1883 unreachable"
    exit 1
fi
ok "broker reachable"

# The image must be fetchable from where the *device* is, not from here, but a
# failure here is still worth knowing about before the device tries.
IMG_HEADERS="$(curl -s -o /dev/null -D - --max-time 10 "$FIRMWARE_URL" 2>/dev/null)"
IMG_CODE="$(printf '%s' "$IMG_HEADERS" | head -1)"
if printf '%s' "$IMG_CODE" | grep -q ' 200'; then
    IMG_SIZE="$(printf '%s' "$IMG_HEADERS" | grep -i '^content-length:' | tr -d '\r' | awk '{print $2}')"
    ok "image served (${IMG_SIZE:-?} bytes)"
else
    bad "image not fetchable: ${IMG_CODE:-no response}"
    echo
    echo "    Start the server:  tools/ota/serve.py"
    exit 1
fi

STATUS="$(retained "$BASE/status")"
if [ -z "$STATUS" ]; then
    bad "device has no retained status -- is it online?"
    exit 1
fi
BEFORE_FW="$(printf '%s' "$STATUS" | jget firmware_version)"
BEFORE_IP="$(printf '%s' "$STATUS" | jget ip)"
ok "device online, firmware_version = ${BEFORE_FW:-?}  (ip ${BEFORE_IP:-?})"

# --- trigger -----------------------------------------------------------------
echo
echo "2. Trigger OTA"

# Subscribe before publishing, or early progress lines are missed.
( timeout 120 mosquitto_sub -h "$BROKER" -t "$BASE/response" -v -W 118 \
  > /tmp/_ota_responses 2>/dev/null ) &
SUB_PID=$!
sleep 1

mosquitto_pub -h "$BROKER" -t "$BASE/command" \
    -m "{\"command\":\"ota\",\"url\":\"$FIRMWARE_URL\"}" 2>/dev/null
ok "ota command published"

# --- watch -------------------------------------------------------------------
echo
echo "3. Progress"

DEADLINE=$((SECONDS + 100))
LAST_PCT=""
REBOOTED=0
while [ $SECONDS -lt $DEADLINE ]; do
    sleep 2
    LINE="$(grep -o '{.*}' /tmp/_ota_responses 2>/dev/null | tail -1)"
    [ -z "$LINE" ] && continue

    PCT="$(printf '%s' "$LINE" | jget percent)"
    STATE="$(printf '%s' "$LINE" | jget status)"

    if [ -n "$PCT" ] && [ "$PCT" != "$LAST_PCT" ]; then
        LAST_PCT="$PCT"
        printf '        %s%%\n' "$PCT"
    fi
    if [ "$STATE" = "failed" ]; then
        bad "device reported the update failed"
        break
    fi
done
wait $SUB_PID 2>/dev/null

if [ -z "$LAST_PCT" ]; then
    warn "no progress messages received"
else
    ok "progress reached ${LAST_PCT}%"
    [ "$LAST_PCT" = "100" ] && ok "download completed" || warn "progress stopped at ${LAST_PCT}%"
fi

# --- verify ------------------------------------------------------------------
echo
echo "4. Did it actually update?"

info "waiting for the device to reboot and reconnect..."
DEADLINE=$((SECONDS + 120))
AFTER_FW=""
while [ $SECONDS -lt $DEADLINE ]; do
    sleep 3
    S="$(retained "$BASE/status")"
    V="$(printf '%s' "$S" | jget firmware_version)"
    # Retained offline, or nothing, means it is mid-reboot.
    if [ -n "$V" ] && [ "$V" != "$BEFORE_FW" ]; then
        AFTER_FW="$V"
        break
    fi
done

echo
if [ -z "$AFTER_FW" ]; then
    bad "device did not come back with a different firmware version"
    info "still reporting: ${BEFORE_FW:-nothing}"
    echo
    echo "    Check the device log. Common causes:"
    echo "      - the URL is not reachable from the device (wrong host or firewall)"
    echo "      - the served image is the same one already running, so the"
    echo "        version string cannot change. Rebuild after a commit to test."
    echo "      - the image does not fit the 4 MB OTA slot"
else
    ok "firmware version changed: $BEFORE_FW -> $AFTER_FW"
    ok "OTA round-trip verified"
fi

echo
echo "-------------------------------------------"
printf "  %d passed, %d failed\n" "$PASS" "$FAIL"
echo "-------------------------------------------"
[ "$FAIL" -gt 0 ] && exit 1
exit 0
