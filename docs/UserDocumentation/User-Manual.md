# ESP32-S3 Smart Home — User Manual

How to install, provision, operate and recover an ESP32-S3 node. Human
procedures, present-state.

**Status: partial.** The firmware builds and passes its image/partition smoke
gate, but **nothing has run on real hardware**. No node has booted, joined a
WiFi network, or contacted a broker. The procedures below are the intended
operations; treat every step as unproven until it has been executed on a board.

This manual is the OPERATE plane's entry point. The **step-by-step detail for
building, flashing, provisioning and OTA lives in the existing Sphinx pages**,
which this manual binds to rather than duplicating:

- [`../getting-started.rst`](../getting-started.rst) — build, flash, provision, troubleshoot
- [`../deployment.rst`](../deployment.rst) — release images, OTA on the bench, monitoring
- [`../index.rst`](../index.rst) — the system concept and current status

Where a procedure below and a bound page disagree, the bound page carries the
detail and the FSD carries the contract; the disagreement is recorded as a
documentation defect in FSD §4.4. The proposed cleanup is in
[`../Method/project/plane-retrofit-plan.md`](../Method/project/plane-retrofit-plan.md).

## Contents

1. [What you need](#1-what-you-need)
2. [Build and flash](#2-build-and-flash)
3. [Provision and first run](#3-provision-and-first-run)
4. [Day-to-day operation](#4-day-to-day-operation)
5. [Reconfiguring a node](#5-reconfiguring-a-node)
6. [Updating firmware over the air](#6-updating-firmware-over-the-air)
7. [Troubleshooting](#7-troubleshooting)
8. [Recovery](#8-recovery)
9. [Replacing and removing a node](#9-replacing-and-removing-a-node)

---

## 1. What you need

**Hardware**

| Item | Notes |
|---|---|
| ESP32-S3-DevKitC-1 **N16R8** | 16 MB flash, 8 MB octal PSRAM. A different board is unsupported. |
| USB cable | Data-capable; the first flash is over USB |
| A 2.4 GHz WiFi network | WPA or WPA2-PSK. The node refuses open networks. |
| A Raspberry Pi on the same subnet | Runs the MQTT broker; the DUT must reach it |
| Mosquitto broker | Port 1883 by default |
| Smart_Server | Optional for firmware work; needed to see the device row and REST API |
| Home Assistant + `RiDDiX/home-assistant-matter-hub` | Needed only for Google Home control |
| One Google Nest speaker or hub | Needed only for voice control |

**Workstation software**

- ESP-IDF **v5.5.1** for `esp32s3` — `./make.sh` sources it itself, so a shell
  that has never run `export.sh` still works
- Python 3.8+, CMake ≥ 3.16, Ninja, Git
- Optional: Sphinx + Doxygen to build the docs, `cppcheck` for static analysis

**Credentials and where they live.** Nothing device-specific is compiled in.
The WiFi SSID/passphrase and the MQTT coordinates live in the node's NVS and are
set over the serial console. Keep the passphrase to hand; it is never displayed
back by any command.

## 2. Build and flash

Follow [`../getting-started.rst`](../getting-started.rst) for the full
walkthrough. The sequence is:

```bash
./make.sh setup          # check the toolchain, install Python deps
./make.sh build          # build for ESP32-S3
./make.sh smoke          # verify the image before flashing — do not skip this
./make.sh flash-monitor  # flash and open the serial console
```

`./make.sh smoke` is a real gate: it checks the target is `esp32s3`, the flash
size is 16 MB, PSRAM is **octal**, INFO logging is compiled in, the generated
partition table contains `otadata`/`ota_0`/`ota_1`, and the image fits a 4 MB
slot. A wrong PSRAM mode produces a **boot loop**, not a build error, so this
check is the cheap way to catch it.

Detailed release-image and bench-OTA instructions are in
[`../deployment.rst`](../deployment.rst).

## 3. Provision and first run

Provisioning is over the serial console at 115200 baud. The prompt is `esp32>`.
Run `help` at any point for the grouped command list.

```
esp32> wifi_set <ssid> <password>
esp32> mqtt_set <broker-host> [port]        # port defaults to 1883
esp32> mqtt_auth <username> <password>      # omit for an anonymous broker
esp32> mqtt_device <device_id> <name> [room]
esp32> reboot
```

Notes that prevent the common mistakes:

- **`device_id`** must be lowercase letters, digits, `-` or `_`, at most 24
  characters. It becomes an MQTT topic segment, so anything else is rejected
  rather than silently producing topics that never match.
- **If you skip `mqtt_device`**, the node derives `shnode-<6 hex digits>` from
  its factory MAC and stores it. That is stable across reboots.
- **Every configuration command says "restart to apply"** because it does —
  the value is read once, at boot.
- **`mqtt_status`** shows what is stored and never the password.

On the first boot after provisioning, expect association, an IP address, and a
broker connection in the log:

```
I (4210) Application: Got IP 192.168.1.42
I (4480) MqttService: Connected to 192.168.1.10:1883
```

> **In this build the node does not publish Home Assistant discovery**, so it
> does not appear in Home Assistant automatically. `docs/getting-started.rst`
> says otherwise; that page is stale (FSD OD-7). Discovery returns in the phase
> that settles the Google Home path.

## 4. Day-to-day operation

Once running, a node publishes on its own and answers a small command set.

**What it publishes**

| Topic | Contents |
|---|---|
| `smart_home/devices/<device_id>/status` | The retained device document: `status`, `device_type`, `name`, `firmware_version`, `ip`, `room`, `uptime_s`, `heap`, `rssi` |
| `smart_home/devices/<device_id>/sensor/rssi` | `{"value":-63.00,"unit":"dBm"}` every 30 s |
| `smart_home/devices/<device_id>/sensor/heap` | Free internal heap in bytes, every 30 s |
| `smart_home/devices/<device_id>/sensor/uptime` | Seconds since boot, every 30 s |
| `smart_home/devices/<device_id>/response` | Command acknowledgements and OTA progress |

**Watch it from the Pi**

```bash
mosquitto_sub -h <broker> -t 'smart_home/devices/<device_id>/#' -v
```

**Send a command**

```bash
mosquitto_pub -h <broker> -t smart_home/devices/<device_id>/command \
  -m '{"command":"get_status"}'
mosquitto_pub -h <broker> -t smart_home/devices/<device_id>/command \
  -m '{"command":"reboot"}'
```

The handler understands `get_status`, `reboot` and `ota`. An unrecognised verb
is acknowledged with `"status":"error"`. A body that is not valid JSON, or that
has no `command` field, produces **no response at all** — silence is the
expected outcome there, not a fault.

The node's status topic is retained and carries a Last Will, so if it loses
power the broker publishes `{"status":"offline"}` on its behalf within about
45 seconds.

A scripted end-to-end check of all of the above is
`tools/mqtt/test-node.sh <device_id> [broker_host]`.

**Google Home.** Control through Google Home depends on Home Assistant and the
Matter bridge, which are not part of the firmware and are not yet installed.
Until then a node is visible on MQTT and in Smart_Server only.

## 5. Reconfiguring a node

Configuration is read once, at boot.

1. Attach the serial console.
2. Change the field with its command — for example
   `mqtt_set 192.168.1.10 1883`, `mqtt_device kitchen-01 Kitchen kitchen`, or
   `mqtt_ota_url https://host/fw.bin`.
3. `reboot`.

**Changing WiFi credentials:**

```
esp32> wifi_set <new-ssid> <new-password>
esp32> reboot
```

**Erasing MQTT configuration:**

```
esp32> mqtt_clear
esp32> reboot
```

This erases the stored keys, so the node boots without a broker and re-derives
its device id.

> **`wifi_clear` is known to be incomplete in this build.** It writes empty
> strings rather than erasing the keys, so the node still reports itself as
> provisioned and attempts association with an empty SSID. Use `wifi_set` to
> replace the credentials instead of clearing them, and see FSD OD-4.

## 6. Updating firmware over the air

Updates are triggered over MQTT and written to the inactive slot; the bootloader
swaps on the next reset.

```bash
mosquitto_pub -h <broker> -t smart_home/devices/<device_id>/command \
  -m '{"command":"ota","url":"https://host/firmware.bin"}'
```

If a default URL is stored (`mqtt_ota_url`), omit the `url` field to use it.
Progress arrives on the response topic as
`{"command":"ota","status":"progress","percent":N}`, ending at 100 immediately
before the node restarts. On success the node reboots into the new image; on
failure it logs an OTA error and keeps running the current image.

Three things to know before relying on OTA:

- **HTTPS needs a correct clock, and the node has no RTC and no time
  synchronisation.** Until SNTP is added, every HTTPS certificate looks
  not-yet-valid and the handshake fails. Use the plain-HTTP bench image
  (`./make.sh build-ota-test`, documented in
  [`../deployment.rst`](../deployment.rst)) on a network you control, and
  remember to return to `./make.sh build-production` afterwards.
- **Automatic rollback is not enabled.** An image that transfers correctly but
  crashes at boot will stay in place. Have a USB reflash available.
- **A stale image proves nothing.** The firmware version comes from the git
  revision, so commit a change before building the second image, or the update
  will "succeed" without any visible difference.

## 7. Troubleshooting

| Symptom | Likely cause | What to do |
|---|---|---|
| Board boot-loops immediately | Wrong PSRAM mode (quad instead of octal) | Confirm `CONFIG_SPIRAM_MODE_OCT=y`; rebuild |
| `idf.py: command not found` | ESP-IDF not sourced — but `./make.sh` sources it itself | Use `./make.sh build` from any shell |
| No `esp32>` prompt | Wrong baud, or the port is not UART0 | 115200 8N1; check the USB cable is data-capable |
| Boots but never associates | No or stale WiFi credentials | `wifi_status`, then `wifi_set` and reboot |
| Association retries stop after a while | 10 attempts exhausted; the node stops until reset | Fix the SSID/AP and reboot |
| Boots, associates, but no broker connection | Missing or wrong `mqtt_set` | `mqtt_status`; `mqtt_set <host> 1883`; reboot |
| Nothing on the broker | Wrong device id in your subscribe topic | The prefix is `smart_home/devices/<device_id>/` |
| No telemetry but status is online | Broker was unreachable; readings are never buffered | Readings resume on the next 30 s cycle |
| Nothing appears in Home Assistant | Discovery is not published in this build | Expected; see §3 |
| Command gets no reply | The body is not valid JSON or has no `command` field | Check the payload; check the response topic |
| OTA over HTTPS fails with a trust error | No clock synchronisation | Use the bench HTTP image, or accept the gap |
| Build fails after a pull | Stale build directory | `./make.sh clean && ./make.sh build` |

The node's own diagnostics: `version` prints the firmware version, build time,
IDF version, uptime and the **reset reason**. After an OTA, a reset reason of
`power-on` means the new image never ran.

## 8. Recovery

**The serial console is always available**, even with no WiFi or MQTT
configuration, so a node that is not on the network is still recoverable over
USB/UART.

| Situation | Recovery |
|---|---|
| Wrong WiFi/broker configuration | Re-provision over the console and reboot |
| Node unreachable on the network | Attach the console; the REPL does not depend on the network |
| Boot loop after an OTA | Reflash over USB with `./make.sh flash-monitor`. Automatic rollback is **not** enabled |
| NVS corrupted | The firmware erases NVS and reinitialises on `NO_FREE_PAGES`/`NEW_VERSION_FOUND`; otherwise erase the flash and re-provision |
| Wrong board flashed from a shared build | Configuration is per-device in NVS; re-provision the identity |

What happens **automatically** (do not intervene):

- WiFi re-association, up to 10 attempts at 5 s intervals.
- MQTT reconnection with exponential backoff from 2 s to 60 s, indefinitely.
- NVS erase-and-retry on a corrupt NVS partition.

What needs a **person**:

- Re-provisioning after `wifi_clear`, `mqtt_clear`, an NVS erase, or a board
  replaced with a different unit.
- Reflashing over USB after a bad image.
- Erasing retained topics on the broker after removing a node.

## 9. Replacing and removing a node

**Replace a board, keep the identity.** Because identity lives in NVS rather
than the image, provision the new board with the same id:

```
esp32> mqtt_device <same-device-id> <same-name> <same-room>
```

Consumers that key on the device id treat it as the same device. Note that the
new board republishes the retained topics it owns.

**Remove a node.** Erase its retained topics, or they reappear as stale entries:

```bash
for t in status sensor/rssi sensor/heap sensor/uptime; do
  mosquitto_pub -h <broker> -r -n -t "smart_home/devices/<device_id>/$t"
done
```

> The command above uses the topic schema the firmware actually publishes
> (`smart_home/devices/<id>/sensor/<ch>`). `docs/deployment.rst` shows the older
> `smart_home/<id>/<ch>/state` form; that page is stale (FSD OD-6).

---

**Contract:** [`../Functionality/FSD.md`](../Functionality/FSD.md) ·
**Build rules:** [`../Method/AI-Workflow.md`](../Method/AI-Workflow.md) ·
**Bound steps:** [`../getting-started.rst`](../getting-started.rst),
[`../deployment.rst`](../deployment.rst)
