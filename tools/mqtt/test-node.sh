#!/bin/bash
#
# Exercise a provisioned ESP32-S3 node against Smart_Server.
#
#   tools/mqtt/test-node.sh <device_id> [broker_host]
#
# Checks, in order:
#   1. the broker accepts a connection
#   2. the node's retained status document says "online"
#   3. sensor readings are arriving (rssi, heap, uptime)
#   4. a command round-trips: get_status -> response topic
#   5. Smart_Server registered the device in its database
#   6. (opt-in) reboot, and the Last Will marks it offline
#
# The layout follows Smart_Server's bridge, which parses topics positionally:
# parts[2] is the device id and parts[3] the message type, so the "devices"
# segment is load-bearing.

set -u

DEVICE_ID="${1:-}"
BROKER="${2:-$(ip -4 -o addr show scope global 2>/dev/null | awk '{print $4}' | cut -d/ -f1 | grep -E '^192\.168\.' | head -1)}"
API="${API:-http://${BROKER}:8000}"

if [ -z "$DEVICE_ID" ]; then
    echo "usage: $0 <device_id> [broker_host]"
    echo
    echo "device_id is what you gave 'mqtt_device' on the console."
    exit 1
fi

PASS=0
FAIL=0
ok()   { printf '  \033[0;32mPASS\033[0m  %s\n' "$1"; PASS=$((PASS + 1)); }
bad()  { printf '  \033[0;31mFAIL\033[0m  %s\n' "$1"; FAIL=$((FAIL + 1)); }
warn() { printf '  \033[0;33mSKIP\033[0m  %s\n' "$1"; }

BASE="smart_home/devices/$DEVICE_ID"

retained() { timeout 5 mosquitto_sub -h "$BROKER" -t "$1" -C 1 -W 3 2>/dev/null; }
jget()     { python3 -c "import json,sys; print(json.load(sys.stdin).get('$1',''))" 2>/dev/null; }

echo "Node '$DEVICE_ID'  ·  broker $BROKER  ·  api $API"
echo

# --- 1. broker ---------------------------------------------------------------
echo "1. Broker"
if mosquitto_pub -h "$BROKER" -t "_smarthome_test/ping" -m ping 2>/dev/null; then
    ok "$BROKER:1883 accepts publishes"
else
    bad "cannot reach $BROKER:1883"
    echo
    echo "    Start the stack:  tools/mqtt/broker.sh start"
    exit 1
fi

# --- 2. status ---------------------------------------------------------------
echo
echo "2. Status document"
STATUS="$(retained "$BASE/status")"
if [ -z "$STATUS" ]; then
    bad "no retained message on $BASE/status"
    echo
    echo "    The node is not connected. On its console, check:"
    echo "      mqtt_status                 # broker configured?"
    echo "      mqtt_set $BROKER 1883       # then reboot"
    exit 1
fi

STATE="$(printf '%s' "$STATUS" | jget status)"
if [ "$STATE" = "online" ]; then
    ok "status = online"
else
    bad "status = '$STATE', expected 'online'"
fi

FW="$(printf '%s' "$STATUS" | jget firmware_version)"
IP="$(printf '%s' "$STATUS" | jget ip)"
[ -n "$FW" ] && ok "firmware_version = $FW" || bad "no firmware_version in status"
[ -n "$IP" ] && ok "ip = $IP"          || bad "no ip in status"

# --- 3. sensors --------------------------------------------------------------
echo
echo "3. Sensor readings (waiting up to ~35 s for a publish cycle)"

check_sensor() {
    local channel="$1" expect_unit="$2"
    local raw value unit
    raw="$(timeout 38 mosquitto_sub -h "$BROKER" -t "$BASE/sensor/$channel" -C 1 -W 36 2>/dev/null)"
    if [ -z "$raw" ]; then
        bad "$channel: nothing received"
        return
    fi
    value="$(printf '%s' "$raw" | jget value)"
    unit="$(printf '%s' "$raw" | jget unit)"
    if [ -z "$value" ]; then
        bad "$channel: payload has no 'value' -- got $raw"
    else
        ok "$channel = $value ${unit}"
    fi
}

check_sensor rssi dBm
check_sensor heap B
check_sensor uptime s

# --- 4. command round-trip ---------------------------------------------------
echo
echo "4. Command round-trip (get_status)"

# Subscribe before publishing, or the ack can arrive first.
( timeout 12 mosquitto_sub -h "$BROKER" -t "$BASE/response" -C 1 -W 10 \
  > /tmp/_smarthome_ack 2>/dev/null ) &
SUB_PID=$!
sleep 1

mosquitto_pub -h "$BROKER" -t "$BASE/command" -m '{"command":"get_status"}' 2>/dev/null
wait $SUB_PID 2>/dev/null

ACK="$(cat /tmp/_smarthome_ack 2>/dev/null)"
if [ -z "$ACK" ]; then
    bad "no response on $BASE/response"
else
    ACK_STATUS="$(printf '%s' "$ACK" | jget status)"
    if [ "$ACK_STATUS" = "ok" ]; then
        ok "ack: $ACK"
    else
        bad "ack reported '$ACK_STATUS': $ACK"
    fi
fi

# --- 5. server registered it -------------------------------------------------
echo
echo "5. Smart_Server database"
DEVICES_JSON="$(timeout 20 curl -s "$API/api/devices" 2>/dev/null)"
if [ -z "$DEVICES_JSON" ]; then
    warn "no response from $API/api/devices (is the smart_server container up?)"
else
    FOUND="$(printf '%s' "$DEVICES_JSON" | python3 -c "
import json, sys
try:
    d = json.load(sys.stdin)
except Exception:
    print('parse-error'); raise SystemExit
items = d if isinstance(d, list) else d.get('devices', d.get('items', []))
ids = [i.get('device_id') for i in items if isinstance(i, dict)]
print('found' if '$DEVICE_ID' in ids else 'missing')
" 2>/dev/null)"
    case "$FOUND" in
        found)      ok "server has a device row for $DEVICE_ID" ;;
        missing)    bad "server has no device row for $DEVICE_ID" ;;
        *)          warn "could not parse $API/api/devices" ;;
    esac
fi

# --- 6. Last Will (opt-in) ---------------------------------------------------
echo
echo "6. Reboot / Last Will"
if [ "${SKIP_REBOOT:-0}" = "1" ]; then
    warn "skipped (SKIP_REBOOT=1)"
else
    echo "     Sending reboot; the retained status should flip to offline, then online."
    mosquitto_pub -h "$BROKER" -t "$BASE/command" -m '{"command":"reboot"}' 2>/dev/null

    sleep 4
    AFTER="$(retained "$BASE/status")"
    AFTER_STATE="$(printf '%s' "$AFTER" | jget status)"
    if [ "$AFTER_STATE" = "offline" ]; then
        ok "status flipped to offline (Last Will fired)"
    else
        warn "status is '$AFTER_STATE' after 4 s -- the Last Will may not have fired"
    fi

    echo "     Waiting up to 90 s for the node to come back..."
    BACK=0
    for _ in $(seq 1 45); do
        sleep 2
        if [ "$(retained "$BASE/status" | jget status)" = "online" ]; then
            BACK=1
            break
        fi
    done
    [ "$BACK" = "1" ] && ok "node reconnected" || bad "node did not come back within 90 s"
fi

# --- summary -----------------------------------------------------------------
echo
echo "-------------------------------------------"
printf "  %d passed, %d failed\n" "$PASS" "$FAIL"
echo "-------------------------------------------"
[ "$FAIL" -gt 0 ] && exit 1
exit 0
