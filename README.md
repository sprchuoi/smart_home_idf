# ESP32-S3 Smart Home

Firmware for ESP32-S3 sensor and actuator nodes that appear in Google Home,
via Home Assistant and a local Matter bridge.

[![CI](https://github.com/sprchuoi/smart_home_idf/actions/workflows/ci.yml/badge.svg)](https://github.com/sprchuoi/smart_home_idf/actions/workflows/ci.yml)
[![Documentation](https://github.com/sprchuoi/smart_home_idf/actions/workflows/docs.yml/badge.svg)](https://github.com/sprchuoi/smart_home_idf/actions/workflows/docs.yml)

📖 **[Documentation](https://sprchuoi.github.io/smart_home_idf/)** · 📚 **[API reference](https://sprchuoi.github.io/smart_home_idf/doxygen/)** · 🗺 **[Roadmap](ROADMAP.md)**

## The idea

The obvious way to get a DIY device into Google Home is to run Matter on the
microcontroller. That works, but it is the hard road — and unnecessary.

**Google Home only ever needs to see a Matter bridge.** How that bridge gets
its data is irrelevant to Google. So the node speaks plain MQTT and a Raspberry
Pi does the bridging:

```
        Google Home app  ·  Nest speaker
                    │  Matter (local only)
        ┌───────────▼─────────────────────────────┐
        │  Raspberry Pi                            │
        │    Mosquitto   ← MQTT broker             │
        │    Home Assistant                        │
        │      └─ Matter bridge add-on             │
        └───────────┬──────────────────────────────┘
                    │  MQTT over WiFi 2.4 GHz
        ┌───────────▼──────────────────────────────┐
        │  ESP32-S3 nodes — sensors + actuators    │
        └──────────────────────────────────────────┘
```

No cloud project, no OAuth server, no public endpoint, nothing exposed to the
internet, and no recurring cost. See [the docs](https://sprchuoi.github.io/smart_home_idf/)
for the full rationale.

## What it does today

- Targets **ESP32-S3** (DevKitC-1 N16R8: 16 MB flash, 8 MB octal PSRAM)
- Connects to WiFi, with credentials provisioned over the serial console into NVS
- Publishes to MQTT with a Last Will, exponential-backoff reconnect, and per-class QoS
- Announces itself to Home Assistant via MQTT discovery
- Reports link diagnostics: RSSI, free heap, uptime, connectivity
- Supports HTTPS OTA into a dual-slot partition table

**Not yet on hardware.** Everything above is verified at build level only — see
the verification checklist in [ROADMAP.md](ROADMAP.md).

## Quick start

```bash
./make.sh setup          # check toolchain, install Python deps
./make.sh build          # build for ESP32-S3
./make.sh smoke          # verify the image before flashing
./make.sh flash-monitor  # flash and open the serial console
```

Then provision over that console:

```
esp32> wifi_set <ssid> <password>
esp32> mqtt_set <broker-host> [port]
esp32> mqtt_device <device_id> <name> [room]
esp32> reboot
```

Your device then appears in Home Assistant automatically. Full walkthrough in
[Getting Started](https://sprchuoi.github.io/smart_home_idf/getting-started.html).

## Layout

```
main/
├── app/          Application orchestrator — owns services, wires callbacks
├── core/         Application state
├── services/     WiFi, MQTT, OTA — each with its own NVS config
├── drivers/      UART
└── error/        Error logging and counters
docs/             Sphinx user guide + Doxygen API reference
```

## Build commands

| Command | Purpose |
|---|---|
| `./make.sh setup` | Check toolchain, install dependencies |
| `./make.sh build` | Build firmware |
| `./make.sh smoke` | Verify the image (target, PSRAM, log level, OTA slots, size) |
| `./make.sh flash-monitor` | Flash and open the serial console |
| `./make.sh test` | Static analysis |
| `./make.sh doc` | Build the documentation |
| `./make.sh ci` | Run the full pipeline locally |

## Deliberately absent

Recorded so their absence does not read as an oversight:

- **Audio and wake word.** Removed. No working wake-word model existed, and the
  audio pipeline did not compile for the ESP32-S3.
- **Power management.** Removed. Only `NORMAL` was implemented, and WiFi nodes
  are not battery powered.
- **OLED display.** Removed. Its render path was a stub with no framebuffer.

## Licence

Provided as-is for educational and professional use.
