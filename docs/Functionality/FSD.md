# ESP32-S3 Smart Home — Functional Specification Document (FSD)

**System under specification:** the ESP32-S3 firmware in this repository (the
device under test, DUT). Smart_Server, the Raspberry Pi, the MQTT broker, Home
Assistant and Google Home are declared external interfaces, not specified
internals.

**Verification status:** nothing in this document has been verified on hardware.
The DUT has never run on a board. Evidence today is build-level only — the
image compiles and `./make.sh smoke` passes its target, flash-size, PSRAM-mode,
log-level, partition-label and image-size checks. Every requirement below names
the tier that would prove it and is recorded as currently unproven.

**Requirement convention:** `FR-<chapter>.<n>` / `NFR-<chapter>.<n>` with
`[Must]`, `[Should]` or `[May]`, "shall" language, stated in the chapter of the
component it constrains. IDs are stable and are never renumbered. Provenance
tags: `[user]`, `[derived]`, `[code]`, `[pack:esp32]`. Status tags:
`approved`, `detected-in-code`, `proposed`, `deprecated`, `pending`.

---

## 1. System Overview

### 1.1 Purpose

Provide low-cost sensor and actuator nodes that appear in Google Home, using a
Raspberry Pi as the hub. The node's job is deliberately narrow: join a 2.4 GHz
WiFi network, speak plain MQTT to a local broker, announce and update its own
state, accept a small command set, and update its own firmware over the air.

The Google Home route is settled `[user]`: Home Assistant runs alongside
Smart_Server on the Pi, and the `RiDDiX/home-assistant-matter-hub` add-on is the
Matter bridge. The firmware does **not** speak Matter, Thread, or any Google
protocol, and shall not be required to (NFR-18.4). Rejected alternatives —
running Matter on the microcontroller, an Actions-on-Google/OAuth route, and a
Matter bridge embedded in Smart_Server — are recorded in §4.5.

### 1.2 Problem statement

A DIY device normally reaches Google Home by running a full Matter stack on the
microcontroller. That consumes the flash and RAM the application needs and adds
a cloud or OAuth dependency. Google Home only ever sees a Matter **bridge**;
how that bridge obtains its data is invisible to it. Putting the bridge on a
Raspberry Pi and leaving the node as a small MQTT client keeps control local,
costs nothing recurring, and exposes nothing to the internet.

### 1.3 Users and stakeholders

| Role | Interest |
|---|---|
| Household user | Controls and reads the node through Google Home |
| Installer / operator | Flashes, provisions, diagnoses and recovers nodes over the serial console |
| Firmware maintainer | Changes the firmware without breaking the MQTT contract Smart_Server depends on |
| Reviewer / tester | Judges whether each requirement is met, from the contracts in this document |

### 1.4 Goals and non-goals

**Goals**

- One firmware image serves every node; nothing device-specific is compiled in.
- A node's behaviour is observable end to end over MQTT without sensor hardware.
- Configuration lives in NVS and is provisioned over the serial console.
- A node recovers from WiFi and broker loss without a person and without a reboot.

**Non-goals**

- Specifying Smart_Server, Home Assistant, the broker, or the Matter bridge.
- Running a Matter/Thread/Google protocol stack on the microcontroller.
- Sensor and actuator device drivers — Phase 3 and Phase 5 work, not yet specified.
- Battery operation and power management (see §4.5).
- On-node audio capture and wake-word detection (see §4.5).

### 1.5 High-level system flow

```
  Google Home app / Nest speaker
            |  Matter (local)
  +---------v----------------------------------------------+
  |  Raspberry Pi                                          |
  |    Mosquitto  <- MQTT broker on port 1883              |
  |    Smart_Server (FastAPI + database + REST)            |
  |    Home Assistant + home-assistant-matter-hub          |
  +---------+----------------------------------------------+
            |  MQTT over WiFi 2.4 GHz, plain TCP
  +---------v----------------------------------------------+
  |  ESP32-S3 node (the DUT)                               |
  |    console -> NVS provisioning -> WiFi -> MQTT         |
  |      status (retained) | sensor/<ch> | command/response|
  |    HTTPS OTA into the inactive slot                    |
  +--------------------------------------------------------+
```

The DUT is the box at the bottom. Everything above the MQTT wire is external.

### 1.6 Current verification status

| Capability | Evidence that exists | Evidence that does not |
|---|---|---|
| Compiles for esp32s3 with the committed partition table | CI build + `./make.sh smoke` | — |
| Image fits a 4 MB OTA slot (838 KB, `[code]`) | `./make.sh smoke`, CI image check | On-target boot |
| Target, flash size, PSRAM mode, log level in the generated image | `./make.sh smoke`, CI | On-target boot |
| Partition labels `otadata`, `ota_0`, `ota_1` in the generated table | `./make.sh smoke`, CI | Flash write, slot swap |
| Every behaviour in this FSD | — | Any board run |

**Hardware verification has not been performed.** No requirement in this
document may be reported as verified, and no document may describe a behaviour
as "verified on hardware", until the ROADMAP hardware checklist has been
executed on a real board. Where another repository document asserts hardware
verification, this FSD and the checklist in `ROADMAP.md` are correct and that
assertion is a documentation defect (§4.4, OD-8).

---

## 2. System Architecture

### 2.1 Logical architecture

The firmware is a single FreeRTOS application with four long-lived execution
contexts and one composition root.

| Context | Owner | Responsibility |
|---|---|---|
| `app_main` (main task) | `main/main.cpp` | Construct, initialise, hand control to `Application::run()` |
| `WifiService` task | `main/services/wifi` | Drain the WiFi event queue, drive reconnection |
| `MqttService` task | `main/services/mqtt` | Own the MQTT reconnect loop; publishes and subscribes via the esp-mqtt client |
| `OTAService` task | `main/services/ota` | Perform an HTTPS OTA transfer when requested |
| esp-mqtt internal task | esp-mqtt component | Transport, keepalive, dispatch of inbound messages |

`Application` is the only object that knows about more than one service. It
constructs the services, wires their callbacks, and owns their lifetime.
Services communicate through **direct callbacks**, one listener per edge — not
through a shared event bus. The edges are:

| From | To | Trigger |
|---|---|---|
| `WifiService` | `Application::onWifiEvent` | STA start, got IP, disconnected |
| `MqttService` | `Application::onMqttConnection` | Broker connect / disconnect |
| `MqttService` | `Application::onMqttCommand` | A message on the command topic |
| `OTAService` | `Application` progress lambda | Download percentage change |

### 2.2 Hardware and platform architecture

| Item | Value | Provenance |
|---|---|---|
| Target SoC | ESP32-S3 | `[user]` |
| Board | ESP32-S3-DevKitC-1 N16R8 | `[user]` |
| Flash | 16 MB | `[code]` `sdkconfig.defaults`, ROADMAP |
| PSRAM | 8 MB octal, 80 MHz | `[code]` `sdkconfig.defaults` |
| Bootloader offset | `0x0` (ESP32-S3; `0x1000` is ESP32/S2 only) | `[code]` ROADMAP, partitions.csv |
| Partition table offset | `0x8000` | `[code]` partitions.csv |
| Console port | UART0, 115200 baud, 8N1 | `[code]` `ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT` |
| Usable GPIO | 0–25 and 38–48; strapping pins 0/3/45/46 avoided | `[user]` ROADMAP |
| Connectivity | WiFi station, 2.4 GHz, WPA/WPA2-PSK | `[code]` WifiService |
| Power | Mains / USB; battery operation is out of scope | `[user]` ROADMAP |

The GPIO budget exists because the N16R8's octal PSRAM/SPI0/1 consumes GPIO26–37
(ROADMAP "Verified findings"). It is a constraint the build reads, stated
normatively in §19.

### 2.3 Software architecture

| Concern | Implementation |
|---|---|
| Entry point | `app_main()` constructs a static `Application`, calls `initialize()` then `run()` |
| Provisioning | `WifiConfigInterface` and `MqttConfigInterface` singletons over NVS, exposed as `esp_console` commands |
| Console | `esp_console` REPL on UART0; `console::addFeature()` groups commands by feature; grouped `help` replaces the stock one |
| Persistence | Two NVS namespaces, `wifi_config` and `mqtt_config` |
| Update model | `esp_https_ota` incremental API into the inactive slot; bootloader swaps on the next reset |
| Watchdog | ESP-IDF TWDT, 30 s, fed by the MQTT and OTA tasks |
| Error handling | `ErrorHandler` singleton: one `ESP_LOGE` line per error plus per-category counters |
| JSON | cJSON for encoding response bodies and parsing inbound commands |

### 2.4 Component Layering

Strict one-way dependency: each layer depends only on layers below it. The
L0/L1 line is **ownership** — did we implement and test the handler?

| Layer | Contents |
|---|---|
| **L2 Application logic** | `Application` orchestration, telemetry scheduling, command dispatch, the application state holder |
| **L1 Interfaces** | `WifiService` link + reconnect logic, `WifiConfigInterface` store, `MqttService` topic/payload wire and command flow, `MqttConfigInterface` store, `OTAService` receiver, `Console` command registry, `SystemCommands` |
| **L0 Foundation** | ESP-IDF v5.5.1: FreeRTOS, NVS/flash, `esp_netif`/`esp_event`, the esp-mqtt **client**, `esp_https_ota` + esp-tls/CA bundle, `esp_console`, TWDT, cJSON |

```
+-------------------------------------------------------------+
| L2  Application logic                                        |
|   [Application orchestrator] [Telemetry & command dispatch]  |
|   [AppStateMachine]                                          |
+-------------------------------------------------------------+
| L1  Interfaces (hand-written handlers we own)                |
|   [WifiService] [WifiConfig] [MqttService] [MqttConfig]      |
|   [OTAService] [Console registry] [SystemCommands]           |
+-------------------------------------------------------------+
| L0  Foundation (configured and used, not implemented)        |
|   [FreeRTOS] [NVS/flash] [esp_netif/esp_event]               |
|   [esp-mqtt client] [esp_https_ota + esp-tls/CA bundle]      |
|   [esp_console] [TWDT] [cJSON]                               |
+-------------------------------------------------------------+
```

**Source-layout convention (HOW, mirrored here so §2.4 and the tree agree).**
Each component maps to its own module under `main/`: `app/` (L2 composition
root), `core/` (L2 state and the console registry), `services/<name>/` (L1),
`drivers/` (L1), `error/` (L1 cross-cutting helper). Lower layers never depend
on higher ones; where the need appears to invert, it is inverted with a callback
registered at the composition root (`Application`), which is exactly how the
WiFi and MQTT edges work. Each interface's pure core is extracted so the fast
tier can reach it; extracting those free functions is Phase 1 work, since none
exist today (§21.0, OD-13).

### 2.5 System context

**Users / operators:** household user (via Google Home, never touches the DUT
directly); installer/operator (serial console, USB or UART); maintainer.

**External systems the DUT depends on**

| External system | Interface | Direction | What the DUT requires of it |
|---|---|---|---|
| WiFi access point | 802.11 b/g/n, 2.4 GHz, WPA/WPA2-PSK | bidirectional | An SSID/passphrase pair the operator provisioned; a DHCPv4 lease |
| MQTT broker (Mosquitto on the Pi) | MQTT 3.1.1 over plain TCP, default port 1883 | bidirectional | Accept the connection (optionally authenticated), honour QoS 1 and retained messages, hold a persistent session, deliver commands |
| Smart_Server MQTT bridge | MQTT topic layout parsed positionally: `parts[2]` = device id, `parts[3]` = message type, `parts[4:]` = sensor path | inbound to bridge/outbound to DUT | Nothing; the DUT must match its layout |
| OTA image server | HTTP(S), static file | DUT fetches | Serve the image at a URL the DUT is given; HTTPS requires a valid, trusted certificate chain and a correct device clock |
| Home Assistant + HAMH | none — the firmware never contacts it | — | Consumes the MQTT topics it sees on the broker |
| Google Home | none — the firmware never contacts it | — | Consumes the Matter entities HA exposes |

**Physical environment:** indoor residential; mains-powered; a bench environment
during bring-up where the network is not trusted.

**Trust boundaries**

1. **MQTT wire** — plain TCP, no TLS. Anyone on the LAN who can publish to the
   broker is inside the trust boundary today.
2. **Serial console** — physical access to UART0/USB is full control:
   re-provisioning, reboot, and OTA trigger.
3. **OTA endpoint** — the image server and the network path to it; HTTPS with CA
   verification in a production image, plain HTTP only in a bench image.
4. **NVS flash** — plaintext credentials; readable with physical flash access
   because flash encryption and secure boot are not enabled.

**Lifecycle stages:** manufacture (vendor image, no credentials), provisioning
(console → NVS, reboot), operation (WiFi + MQTT + telemetry + commands),
service (OTA update), decommission (retained topics erased on the broker;
`wifi_clear` / `mqtt_clear` on the node).

### 2.6 Build-verified baseline

The committed build produces an image for `esp32s3` with 16 MB flash, octal
PSRAM at 80 MHz, log level INFO compiled in, and a generated partition table
containing `nvs`, `otadata`, `phy_init`, `ota_0`, `ota_1`, `storage`. The image
was 838 KB at the Phase 1 build (`[code]` ROADMAP). Nothing beyond this has been
demonstrated.

---

## 3. Implementation Phases

Phases are read before the component chapters. Each phase delivers across
layers; the requirement IDs it must satisfy are listed so entry and exit are
checkable. **A phase may use only what an earlier phase delivered.**

### 3.1 Phase 0 — Prerequisites and hardware ground truth

- **Scope:** confirm board revision and actual flash/PSRAM size on a physical
  board; obtain one Google Nest speaker or hub; confirm the Pi's OS, storage and
  subnet; confirm router IPv6 on, IGMP snooping off, AP isolation off; record
  the usable GPIO budget.
- **Deliverables:** a bench with one N16R8 board, a USB or UART connection, and
  a Pi on the same L2 subnet.
- **Exit criteria:** the board can be flashed with any known-good firmware and
  its serial log is readable.
- **Dependencies:** none. **This phase has not been started.** It is the reason
  every verification tier below is unproven.

### 3.2 Phase 1 — Infrastructure foundation

- **Scope:** build for esp32s3 with the committed partition table; console REPL;
  NVS; the two config stores and their console commands; the application state
  holder; the image/partition smoke gate.
- **Requirements:** FR-5.1, FR-5.6–FR-5.9, FR-8.1–FR-8.6, FR-8.8, FR-8.9,
  FR-10.1–FR-10.10, FR-12.1–FR-12.11, FR-14.1–FR-14.5, NFR-13.1–NFR-13.9,
  NFR-15.1–NFR-15.6, NFR-17.1–NFR-17.5, NFR-19.1–NFR-19.9.
- **Deliverables:** a flashable image; provisioning commands; an honest smoke gate.
- **Exit criteria:** on hardware, the board boots, prints the banner, presents
  the `esp32>` prompt, and accepts `wifi_set`/`mqtt_set` into NVS; `version`
  reports reset reason `power-on`.
- **Dependencies:** Phase 0.

### 3.3 Phase 2 — WiFi, MQTT and Smart_Server

- **Scope:** WiFi station connect and reconnect; MQTT connect, retained status,
  Last Will, per-class QoS, owned backoff reconnect; command dispatch and
  acknowledgements; telemetry of `rssi`, `heap`, `uptime`.
- **Requirements:** FR-5.2–FR-5.5, FR-5.10–FR-5.12, FR-6.1–FR-6.20,
  FR-7.1–FR-7.7, FR-7.9, FR-9.1–FR-9.18, FR-16.1–FR-16.4, NFR-16.5, NFR-16.6.
- **Deliverables:** a node that appears as a device row in Smart_Server, reports
  online/offline, publishes telemetry, and answers `get_status` and `reboot`.
- **Exit criteria:** the device row appears in Smart_Server's database; pulling
  power flips its status to offline within the keepalive window; a command from
  the server reaches the board and is acknowledged.
- **Dependencies:** Phase 1. Smart_Server's app container must be built where
  the Debian mirrors are reachable; the broker alone is not enough (§4.2).

### 3.4 Phase 3 — Observing the data

- **Scope:** a sensor abstraction (`begin()`, `read()`, descriptor with unit and
  device class); I²C and ADC/GPIO drivers for the sensors on hand; publish on
  change plus a periodic heartbeat; a simulated sensor that uses the identical
  publish path; **settle channel naming here**, before Phase 4.
- **Requirements:** the existing telemetry contract (FR-6.1–FR-6.5) is the
  template; sensor-specific requirements are written when the abstraction is
  designed.
- **Deliverables:** real or simulated sensor values arriving in Smart_Server's
  dashboard on cadence.
- **Exit criteria:** a sensor value updates in the dashboard on cadence and
  survives a broker restart.
- **Dependencies:** Phase 2. Sensor hardware is blocked on parts for the real
  drivers; the simulated sensor is not blocked.

### 3.5 Phase 4 — Google Home

- **Scope:** install Home Assistant and the HAMH add-on on the Pi alongside
  Smart_Server; expose the chosen entities to the Matter bridge; **publish Home
  Assistant MQTT discovery** from the firmware for those entities; commission
  the bridge into Google Home by QR.
- **Requirements:** a new discovery requirement set, written when the entity
  list is fixed; it supersedes the current "discovery is not published"
  behaviour (OD-7). FR-6.5 stays normative — discovery does not change the
  status document.
- **Deliverables:** a Matter bridge on the Pi; retained discovery topics for the
  exposed entities; a commissioned Google Home fabric.
- **Exit criteria:** "Hey Google, what's the temperature in the \<room\>?"
  returns a value originating from the ESP32-S3. This is the headline milestone.
- **Dependencies:** Phase 3 (channel naming), Phase 2 (reliable MQTT), and the
  Phase 0 router prerequisites. **Sequencing check:** every capability this exit
  rests on — naming (Phase 3), MQTT (Phase 2), provisioning (Phase 1) — is
  delivered by an earlier phase. Pass.

### 3.6 Phase 5 — Actuators

- **Scope:** relay/switch control, retained state, QoS 1 commands, command
  acknowledgement tracking rather than assuming the relay moved.
- **Requirements:** extends the command contract (FR-6.6–FR-6.12) with actuator
  verbs; written at design time.
- **Deliverables:** at least one relay/switch node with retained state, an
  acknowledged command path, and a declared actuator vocabulary.
- **Exit criteria:** voice toggles a real load and the HA state matches physical
  reality after a node reboot or broker restart.
- **Dependencies:** Phase 4 for the voice path, Phase 2 for the command path.

### 3.7 Phase 6 — nRF5340 Thread sensor node (deferred)

- **Scope:** a Zephyr + OpenThread sleepy end device; a Home Assistant Thread
  border router with an RCP; pin a Zephyr version first (`smart_home_zephyr`
  tracks `revision: main` with no lock file).
- **Requirements:** a separate FSD for that firmware; not this document.
- **Deliverables:** a pinned Zephyr version and lock file in `smart_home_zephyr`,
  a Thread border router in Home Assistant, and a battery-powered sleepy end
  device publishing sensor data.
- **Exit criteria:** a battery-powered nRF5340 sensor appears in HA and is
  controllable from Google Home.
- **Dependencies:** Phase 4 (Home Assistant present). `[user]` deferred to a
  later phase.

### 3.8 Phase 7 — OTA hardening, TLS and watchdog

- **Scope:** enable and exercise automatic rollback
  (`CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE` is unset today); **SNTP so the
  device clock is valid before an HTTPS handshake**; TLS on MQTT; broker
  authentication off-bench; watchdog coverage for every long-running task.
- **Requirements:** FR-11.1–FR-11.15, NFR-11.13, NFR-11.14, NFR-18.3, NFR-18.4,
  NFR-18.6, NFR-18.9, NFR-18.10; rollback and SNTP requirements are
  written when the values are chosen (OD-9, OD-11).
- **Deliverables:** SNTP time synchronisation before any HTTPS handshake;
  automatic rollback enabled and exercised; TLS on MQTT; broker authentication
  off-bench; watchdog coverage stated for every long-running task.
- **Exit criteria:** an OTA update ships **over HTTPS**, and a deliberately
  broken image rolls back.
- **Dependencies:** Phase 2. **Sequencing defect resolved here:** the ROADMAP's
  Phase 7 exit criterion ("an OTA update ships") cannot be met over HTTPS by any
  earlier phase because certificate validation needs a correct clock and no
  phase delivers time synchronisation. SNTP is therefore made explicit Phase 7
  scope. Without it the exit criterion is unreachable and the phase would arrive
  at the hardware for a missing capability — §4.4, OD-9.

### 3.9 Phase 8 — Voice (parked)

- **Scope:** none. Requires an I²S microphone and a real wake-word model; no
  working wake-word model exists in any of the three repositories.
- **Deliverables:** none.
- **Exit criteria:** none set. Parked by `[user]`.
- **Dependencies:** none; a future decision to unpark it, plus a working
  wake-word model, which does not exist today.

---

## 4. Risks, Assumptions, Dependencies and Out of Scope

### 4.1 Assumptions

| ID | Assumption | Basis |
|---|---|---|
| A-1 | The operator has physical access to the node to provision it over the serial console. | `(assumed)` — no out-of-band provisioning exists |
| A-2 | The WiFi network is 2.4 GHz and secured with WPA or WPA2-PSK. | `(assumed)` from `WIFI_AUTH_WPA_WPA2_PSK` in WifiService |
| A-3 | The MQTT broker is reachable on the same L2 subnet at the configured host and port. | `(assumed)` |
| A-4 | The MQTT broker is configured to accept a persistent session and to retain messages. | `(assumed)` — the protocol requires it for FR-9.3 and FR-9.8 |
| A-5 | The household LAN is trusted, so plaintext MQTT is tolerated. | `(assumed)` — recorded as an accepted risk in §18, not as a security claim |
| A-6 | The OTA image server is reachable by the DUT and serves the image at the URL provided. | `(assumed)` |
| A-7 | Smart_Server's bridge parses topics positionally at `parts[2]`/`parts[3]`/`parts[4:]`. | `[code]` `Smart_Server/app/services/mqtt_bridge.py` |

### 4.2 External dependencies

| Dep | Why it matters | State |
|---|---|---|
| ESP-IDF v5.5.1 | The build pins it; `mqtt` and `json` are bundled in 5.x | `[user]` |
| ESP32-S3-DevKitC-1 N16R8 | The only target; octal PSRAM is mandatory | `[user]` |
| Mosquitto broker | Every MQTT requirement is exercised against it | Partly running on the Pi |
| Smart_Server FastAPI app | Device registration and REST checks | **Container unbuilt** — Debian mirrors unreachable in the build sandbox |
| Home Assistant + HAMH | Phase 4 only | Not installed |
| A testbench Pi + one ESP32 on a USB slot | The `bench` and `target` tiers | **Not available**; no ESP32 is connected |
| Google Nest speaker/hub | Phase 4 exit | Not obtained |

### 4.3 Technical risks

| Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|
| Nothing has run on a board, so every target/bench requirement is unproven | Certain | High | Phase 0 executes the ROADMAP hardware checklist before any phase is called done |
| Wrong PSRAM mode boot-loops instead of failing the build | Medium | High | `./make.sh smoke` and CI assert `CONFIG_SPIRAM_MODE_OCT=y`; §19 states it normatively |
| HTTPS OTA cannot work without a valid clock (no SNTP) | Certain | Medium | Plain-HTTP bench image for now; SNTP moved into Phase 7 scope (§3.8, OD-9) |
| Plaintext MQTT on a shared LAN | Medium | Medium | Accepted for the home LAN; TLS and broker authentication are Phase 7 (§18) |
| No automatic rollback: a bad image persists | Medium | High | Documented as a negative requirement (NFR-11.13); rollback is Phase 7 (OD-11) |
| Smart_Server app container unbuilt, so registration checks are skipped | Certain | Medium | Build where Debian mirrors are reachable; not a firmware defect |
| Broker `allow_anonymous true` | Medium | Medium | Bench-only; §18 requires authentication to be configured off-bench |
| Stale documentation contradicts the code (topic schema, discovery, heartbeat, hardware verification) | Certain | Medium | §4.4 lists each contradiction; the FSD is authoritative (OD-6, OD-7, OD-8, OD-15) |

### 4.4 Repository contradictions found during harvesting

These are recorded so a reader does not resolve them by guessing. Each is an
open decision in §4.6.

1. **Topic schema.** The code and `docs/architecture.rst` use
   `smart_home/devices/<id>/{status,sensor/<ch>,command,response}`. `ROADMAP.md`
   and `docs/deployment.rst` use `smart_home/<id>/{availability,status,<ch>/state,cmd/<target>}`.
   The code matches `Smart_Server/app/services/mqtt_bridge.py`, which parses
   `parts[2]`/`parts[3]`/`parts[4:]` — the code is authoritative (OD-6).
2. **Home Assistant discovery.** `MqttService.h`'s file comment,
   `docs/getting-started.rst` and `docs/index.rst` say discovery topics are
   published and the node "appears in Home Assistant automatically".
   `MqttService::onConnected()` deliberately does **not** publish discovery, and
   `docs/architecture.rst` states it does not. The code is authoritative (OD-7).
3. **Hardware verification.** `ROADMAP.md`'s progress table marks Phase 1
   "verified on hardware" and Phase 2 "firmware verified on hardware". The same
   file's Hardware verification checklist says "Nothing has run on a board", and
   `README.md` says "Not yet on hardware". The checklist and README are
   authoritative; the progress table is wrong (OD-8, `[user]`).
4. **Heartbeat cadence and content.** The ROADMAP checklist says `run()` prints
   `alive | wifi:up | ip:...` every 2 s. The code prints
   `alive | state:... | wifi:... | mqtt:...` every 30 s
   (`TELEMETRY_INTERVAL_MS = 30000`). The code is authoritative (OD-15).
5. **Error diagnosability.** `ErrorHandler.h` says a failing node can be
   diagnosed "from its status topic". The counters are never read or published;
   the status document carries no error fields. Neither behaviour is documented
   as intended elsewhere (OD-3).
6. **Reported application state.** `AppStateMachine.h` says the state "is what
   the MQTT status topic reports". The status document does not contain a state
   field, and nothing publishes the state (OD-2).
7. **`Wifi_cfg.hpp` buffer.** `char ssid[32]` cannot hold a legal 32-character
   SSID; `Mqtt_cfg.hpp` documents the `MAX_LEN + 1` rule that this file breaks
   (OD-5).
8. **Stale Sphinx API/development pages.** `docs/development.rst` names
   `publishState()` and `publishDiscovery()`, neither of which exists;
   `docs/api/` still contains pages for `audio`, `eventbus`, `oled`,
   `powermanager` and `watchdog`. They are not in the `api/index.rst` toctree, so
   the `-W` docs build does not fail, but they describe removed components.

### 4.5 Explicitly out of scope, deprecated or rejected

Recorded so the absence does not read as an oversight. These are **negative
requirements** where a reader might otherwise assume the capability exists.

| Item | Disposition | Reason |
|---|---|---|
| Matter/Thread stack on the microcontroller | **Out of scope** `[user]` | The Pi bridges; the node stays an MQTT client |
| Actions on Google / OAuth / cloud project route | **Rejected** `[user]` | Retired or unmaintained; adds a public endpoint and recurring cost |
| Matter bridge inside Smart_Server | **Rejected** `[user]` | Chosen route is HA + `RiDDiX/home-assistant-matter-hub` |
| Local Home SDK, Google Home APIs | **Rejected** `[user]` | Unmaintained / mobile-app scoped |
| Matter-over-Thread commissioned directly by Google | **Rejected** `[user]` | Requires a Google-certified border router; the HA-bridge route avoids it |
| Audio capture and wake-word detection | **Deprecated / removed** `[user]` | No working wake-word model existed; the pipeline did not compile for the S3 |
| Power management (`PowerManager`) | **Deprecated / removed** `[user]` | Only `NORMAL` existed; WiFi nodes are not battery powered |
| OLED display (`OledDisplay`) | **Deprecated / removed** `[user]` | Render path was a stub with no framebuffer; contained `vTaskDelete(nullptr)` |
| Custom watchdog wrapper (`WatchdogSupervisor`) | **Deprecated / removed** `[user]` | Never programmed the TWDT; IDF's own TWDT is used instead (§13) |
| Publish/subscribe EventBus | **Deprecated / removed** `[user]` | Topology is a DAG; the bus had no consumers and copied payload pointers |
| `UartDriver` second UART | **Out of scope in the current build** | `stop()` deletes UART0, which the console owns; not initialised (`main/app/src/Application.cpp`) |
| `main/services/audio/`, `main/core/audio/`, `main/services/wakeup/` | **Already absent from the tree**; ROADMAP's "Blocked" section is stale | Verified: no such paths exist and no such symbols appear in `main/` |
| nRF5340 Thread sensor node | **Deferred to a later phase** `[user]` | Separate firmware; needs a Thread border router |
| Sensor and actuator drivers | **Phase 3 / Phase 5** | Not yet specified |
| Automatic OTA rollback | **Phase 7**, presently disabled | `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE` unset |
| MQTT TLS and broker authentication | **Phase 7** | Plaintext TCP today; accepted risk on a trusted LAN |
| Secure boot and flash encryption | **Out of scope** | Not enabled; §18 records the accepted risk rather than claiming confidentiality at rest |
| A factory-reset console command | **Out of scope** | `wifi_clear` and `mqtt_clear` are the reset surface |
| Host-side unit tests | **Phase 1 (`/harness`)** | `tests/` is empty; no `linux` target is wired |

### 4.6 Open decisions and un-adopted proposals

Each entry names the requirement it gates and what is blocked. None of these is
a silent adoption.

| ID | Question | Gates | Notes |
|---|---|---|---|
| OD-1 | `AppState::WIFI_CONNECTED`, `OTA_UPDATING` and `ERROR` are declared but no code path enters them. Remove them, or wire them? | FR-5.7 | Either change the enum or add the transitions and their requirements |
| OD-2 | The state holder's header says the state is published on the status topic; nothing publishes it. Publish it, or correct the claim? | §5, §6.5 | Proposal: add a `state` field to the status document |
| OD-3 | `ErrorHandler` counters are never exposed, though the header claims diagnosability from the status topic. Expose them, or correct the claim? | §16 | Proposal: add per-category counts to the status document |
| OD-4 | `wifi_clear` writes empty strings, so `hasCredentials()` stays true and the next boot attempts association with an empty SSID. | FR-8.5, FR-8.9, NFR-15.6 | Code fix required; the MQTT equivalent already erases keys |
| OD-5 | `Wifi_cfg.hpp`'s `ssid[32]` cannot hold a 32-character SSID. | FR-7.7 | Code fix required; follow the `MAX_LEN + 1` rule |
| OD-6 | Which MQTT topic schema is canonical? | FR-9.1–FR-9.6 | Code + `architecture.rst` + Smart_Server agree; ROADMAP and `deployment.rst` are stale |
| OD-7 | When does Home Assistant MQTT discovery return, and for which entities? | §3.5 | Firmware deliberately does not publish it today; Phase 4 owns it |
| OD-8 | The ROADMAP progress table claims hardware verification that did not happen. | all | `[user]`: nothing has run on a board; the table is wrong |
| OD-9 | No SNTP; HTTPS certificate validation fails at the 1970 epoch. Add SNTP, or accept HTTPS OTA as unproven? | FR-11.4, §3.8 | Made explicit Phase 7 scope |
| OD-10 | MQTT is plaintext TCP with no TLS. Accept, or schedule TLS? | §18, NFR-19 | Phase 7 per ROADMAP |
| OD-11 | Automatic OTA rollback is disabled. When is it enabled, and what is the failure threshold? | NFR-11.13 | Threshold value genuinely unknown — must not be invented |
| OD-12 | Are the deleted audio/wakeup/OLED/power/watchdog components formally retired? | §4.5 | They are already absent from the tree; ROADMAP still lists them under "Blocked" |
| OD-13 | Which pure functions become host-tier test seams? | §21.0 | Proposed seams: topic formatting, status/ack JSON construction, device-id validation, sensor payload formatting |
| OD-14 | WiFi reconnect stops permanently after 10 failed attempts until reboot. Is that intended? | FR-7.4 | `detected-in-code`, not documented anywhere |
| OD-15 | The heartbeat cadence and format differ between ROADMAP and code. | NFR-17.4 | Code (30 s, state/wifi/mqtt) is authoritative |
| OD-16 | Broker authentication: `allow_anonymous true` is bench-only. What is the production broker configuration? | NFR-18.6 | Firmware already supports username/password |
| OD-17 | No device-side boot deadline is stated (reset → first steady state). | §3.2 exit criteria | Value genuinely unknown; must not be invented |

### 4.7 Un-adopted pack proposals (ESP32 domain pack, run backwards)

The ESP32 pack detects WiFi STA, captive portal, MQTT, OTA, NVS, watchdog and
logging features in this project. Running it backwards, the following were
detected but are **not** covered by any user decision. None is adopted here;
each needs a user decision before it enters the FSD.

| Proposal | Detected from | Why it might matter | Status |
|---|---|---|---|
| Offline telemetry buffering / replay | MQTT client present | A broker outage currently loses readings | Not adopted — FR-6.16 and FR-9.16 currently specify the opposite |
| Captive-portal provisioning | none in code | Provisioning is console-only; a portal is a common alternative | Not detected in code; not adopted |
| Command acknowledgement tracking | response topic exists | Phase 5 needs proof the actuator moved, not just that the command arrived | Deferred to Phase 5 by `[user]` |
| BLE provisioning/command channel | none in code | An alternative provisioning path | Not detected; not adopted |
| Secure boot / flash encryption | flash present | Protects NVS credentials against physical readout | Not adopted — §18 records the accepted risk |
| HTTP status portal on the node | none in code | Local diagnosis without a broker | Not detected; not adopted |
| mDNS `.local` name | none in code | Discovery without a broker | Not detected; not adopted |
| Device-side boot deadline (reset to first steady state) | boot path present | Makes boot regressions falsifiable | Not adopted — the value must come from a Phase 0 measurement (OD-17); no number is invented here |

---

# Part A — Application Logic (L2)

## 5. Application Lifecycle and State Model

### 5.1 Purpose and scope

`Application` and `AppStateMachine` own the firmware's operational mode. The
mode is process-local and does not survive a reboot. This chapter states the
states, the transitions between them, and the rules that keep them from
contradicting each other.

### 5.2 States

| State | Meaning | Entered today? |
|---|---|---|
| `INIT` | Reset value; before WiFi is started, or when no WiFi credentials are configured | Yes (initial) |
| `WIFI_CONNECTING` | The station is started and association is in progress or being retried | Yes |
| `WIFI_CONNECTED` | Declared in the enum, never entered | **No** — no code path (OD-1) |
| `MQTT_CONNECTING` | An IP address is held; the MQTT client is connecting or reconnecting | Yes |
| `RUNNING` | The broker session is up | Yes |
| `OTA_UPDATING` | Declared in the enum, never entered | **No** — the OTA task does not set it (OD-1) |
| `ERROR` | Declared in the enum, never entered | **No** — no code path (OD-1) |

### 5.3 Transition table (normative)

If this table and the code disagree, the table governs and the code is the
defect. Rows for the three unreachable states are `impossible-by-construction`
and carry the reason.

| From | Event | Guard | To | Action | Limit |
|---|---|---|---|---|---|
| `INIT` | `app_main()` calls `initialize()` | — | `INIT` | Start console REPL, init NVS, open both config stores, register commands | — |
| `INIT` | WiFi credentials present | `WifiService::connect()` returns true | `WIFI_CONNECTING` | Start the station | — |
| `INIT` | WiFi credentials absent | — | `INIT` | Log a provisioning hint; console-only | — |
| `WIFI_CONNECTING` | `WIFI_EVENT_STA_START` | — | `WIFI_CONNECTING` | — | — |
| `WIFI_CONNECTING` | `IP_EVENT_STA_GOT_IP` | — | `MQTT_CONNECTING` | Start the MQTT client | — |
| `WIFI_CONNECTING` | `WIFI_EVENT_STA_DISCONNECTED` | attempts < 10 | `WIFI_CONNECTING` | Call `esp_wifi_connect()` | 5 s between attempts |
| `WIFI_CONNECTING` | `WIFI_EVENT_STA_DISCONNECTED` | attempts ≥ 10 | `WIFI_CONNECTING` | Stop re-attempting until reset | — |
| `MQTT_CONNECTING` | `MQTT_EVENT_CONNECTED` | — | `RUNNING` | Publish the retained status document; subscribe to the command topic | — |
| `MQTT_CONNECTING` | `MQTT_EVENT_DISCONNECTED` | — | `MQTT_CONNECTING` | Notify the MQTT task to schedule a reconnect | backoff 2–60 s |
| `MQTT_CONNECTING` | WiFi lost | — | `WIFI_CONNECTING` | Drop the MQTT session | — |
| `RUNNING` | `MQTT_EVENT_DISCONNECTED` | — | `MQTT_CONNECTING` | Notify the MQTT task to schedule a reconnect | backoff 2–60 s |
| `RUNNING` | WiFi lost | — | `WIFI_CONNECTING` | Call `notifyWifiDown()` so the Last Will fires | — |
| `RUNNING` | `MQTT_EVENT_CONNECTED` | — | `RUNNING` | Re-publish status; re-subscribe | — |
| `WIFI_CONNECTED` | any | — | — | `impossible-by-construction`: no assignment exists (OD-1) | — |
| `OTA_UPDATING` | any | — | — | `impossible-by-construction`: the OTA task never calls `setState` (OD-1) | — |
| `ERROR` | any | — | — | `impossible-by-construction`: no assignment exists (OD-1) | — |

**Unhandled-event default:** events not listed leave the state unchanged. WiFi
events other than `STA_START`, `STA_DISCONNECTED` and `GOT_IP` are discarded by
the handler; MQTT events other than connected/disconnected/data/error do not
touch the state.

**Persistence:** `INIT` is the reset value. No state is written to NVS, so a
reboot always starts from `INIT` (FR-5.8).

### 5.4 Requirements

| ID | Pri | Requirement | Prov / status | Tier |
|---|---|---|---|---|
| FR-5.1 | Must | On boot with WiFi credentials present and `connect()` succeeding, the DUT shall set its state to `WIFI_CONNECTING` before association completes. | `[code]` approved | target |
| FR-5.2 | Must | On acquiring an IPv4 address, the DUT shall set its state to `MQTT_CONNECTING`. | `[code]` approved | target |
| FR-5.3 | Must | On an MQTT `CONNECTED` event, the DUT shall set its state to `RUNNING`. | `[code]` approved | target |
| FR-5.4 | Must | On an MQTT `DISCONNECTED` event while the state is `RUNNING`, the DUT shall set its state to `MQTT_CONNECTING`. | `[code]` approved | target |
| FR-5.5 | Must | On losing the WiFi link while the state is `RUNNING`, the DUT shall set its state to `WIFI_CONNECTING`. | `[code]` approved | target |
| FR-5.6 | Must | On each state change, the DUT shall emit one INFO log line of the form `OLD -> NEW` naming both states. | `[code]` approved | target |
| FR-5.7 | Must | The DUT shall not enter `WIFI_CONNECTED`, `OTA_UPDATING` or `ERROR` in the current build. | `[code]` detected-in-code | target |
| FR-5.8 | Must | After a reset the DUT's state shall be `INIT`, regardless of the state before the reset. | `[code]` approved | target |
| FR-5.9 | Must | The DUT shall remain in `INIT` with no `WIFI_CONNECTING` transition when no WiFi credentials are stored. | `[code]` approved | target |
| FR-5.10 | Must | The DUT shall not restart in response to a WiFi or MQTT disconnect. | `[code]` approved | bench |
| FR-5.11 | Should | The DUT shall reach `WIFI_CONNECTING` within 1 s of a WiFi disconnect being observed. | `[derived]` approved | bench |
| FR-5.12 | Must | On acquiring an IPv4 address, the DUT shall start the MQTT client. | `[code]` approved | target |

### 5.5 Verification contracts

| ID | Precondition · stimulus | Expected observation | Must NOT happen | Tier |
|---|---|---|---|---|
| FR-5.1 | DUT provisioned with a valid SSID/password; reset. | Log shows `INIT -> WIFI_CONNECTING`, then association. | State remains `INIT`; state set after a `GOT_IP` event | target |
| FR-5.2 | DUT connects to the AP. | Log shows `WIFI_CONNECTING -> MQTT_CONNECTING` within 1 s of the IP log line; MQTT client starts. | MQTT started before an IP is held | target |
| FR-5.3 | DUT in `MQTT_CONNECTING`; broker available. | Log shows `MQTT_CONNECTING -> RUNNING` within 5 s of the socket connecting. | `RUNNING` reached without a broker session | target |
| FR-5.4 | DUT in `RUNNING`; stop the broker. | Log shows `RUNNING -> MQTT_CONNECTING` within 5 s. | DUT stays `RUNNING`; DUT restarts | bench |
| FR-5.5 | DUT in `RUNNING`; disable the AP. | Log shows `RUNNING -> WIFI_CONNECTING`. | DUT restarts; state stays `RUNNING` | bench |
| FR-5.6 | Drive any listed transition. | One `AppState:` INFO line `OLD -> NEW` per change; no line when the state is re-set to its current value. | Two lines for one change; a change with no line | target |
| FR-5.7 | Run for 10 min across WiFi loss, broker loss and an OTA attempt. | Only `INIT`, `WIFI_CONNECTING`, `MQTT_CONNECTING`, `RUNNING` appear in logs. | Any log line naming `WIFI_CONNECTED`, `OTA_UPDATING` or `ERROR` | target |
| FR-5.8 | Reach `RUNNING`; reset the board. | First state log is `INIT -> …`; the previous state is not restored. | A transition into `RUNNING` without an intervening `MQTT_CONNECTING` | target |
| FR-5.9 | Erase WiFi credentials; reset. | No `WIFI_CONNECTING` transition; console prompt present. | Association attempt with an empty SSID | target |
| FR-5.10 | Hold the AP down for 60 s and the broker down for 60 s. | Uptime increases monotonically; reset reason on recovery is not `software restart` or `panic`. | A reboot caused by either outage | bench |
| FR-5.11 | Break the AP while `RUNNING`; timestamp the break. | A `RUNNING -> WIFI_CONNECTING` log line within 1 s (±0.2 s). | Transition later than 1.2 s; no transition | bench |
| FR-5.12 | DUT connects to the AP. | An MQTT client is started within 1 s of the IP log line | An MQTT connection attempt before an IP is held | target |

### 5.6 Failure modes and safe states

- **No credentials:** the DUT stays `INIT` and remains console-operable. This is
  the safe state, not an error.
- **WiFi unreachable:** `WIFI_CONNECTING` with bounded retries (FR-7.3, FR-7.4).
  After exhaustion the DUT stays alive and console-operable.
- **Broker unreachable:** `MQTT_CONNECTING` with bounded backoff, retrying
  indefinitely (FR-9.10). Telemetry is dropped, not buffered (FR-9.16).
- **OTA failure:** the state model does not change; the running image continues
  (FR-11.9).

### 5.7 Constants

| Constant | Value | Source |
|---|---|---|
| Wi-Fi reconnect interval | 5000 ms | `[code]` WifiService |
| WiFi reconnect attempt ceiling | 10 | `[code]` WifiService |
| MQTT initial backoff | 2000 ms | `[code]` MqttService |
| MQTT backoff ceiling | 60000 ms | `[code]` MqttService |
| MQTT keepalive | 30 s | `[code]` MqttService |

---

## 6. Telemetry and Command Dispatch

### 6.1 Purpose and scope

This is the L2 decision function that turns device state into MQTT traffic:
periodic diagnostics, the retained status document, and the handling of inbound
commands. It consumes the MQTT interface (§9) and drives it.

### 6.2 Inputs and derivation

| Input | From | Used for |
|---|---|---|
| MQTT connected flag | `MqttService::isConnected()` | Gate all publication |
| AP record (`rssi`) | `esp_wifi_sta_get_ap_info()` | `rssi` reading and status field |
| Free internal heap | `heap_caps_get_free_size(MALLOC_CAP_INTERNAL)` | `heap` reading and status field |
| Uptime | `esp_timer_get_time()` | `uptime_s` reading and status field |
| IPv4 address | `esp_netif_get_ip_info()` on `WIFI_STA_DEF` | Status `ip` field |
| Firmware version | `esp_app_get_description()->version` | Status `firmware_version` field |
| Inbound MQTT `DATA` events | `MqttService` command callback | Command dispatch |

### 6.3 Decision logic

1. Every 30 s, if MQTT is connected, publish `rssi`, `heap` and `uptime` as
   individual sensor readings and log one heartbeat line.
2. Increment a telemetry tick counter each cycle; every 10th cycle, re-publish
   the retained status document (300 s).
3. On MQTT connect, publish the status document once.
4. On a command message: parse JSON; require a string `command` field; dispatch
   `reboot`, `get_status`, `ota`; otherwise acknowledge with `status: error`.
   An unparseable body or a missing `command` field produces no response.

### 6.4 Requirements

| ID | Pri | Requirement | Prov / status | Tier |
|---|---|---|---|---|
| FR-6.1 | Must | While the MQTT session is up, the DUT shall publish one `rssi` reading, one `heap` reading and one `uptime` reading every 30 s. | `[code]` approved | bench |
| FR-6.2 | Must | Each reading shall be published to `smart_home/devices/<device_id>/sensor/<channel>` with QoS 0 and the retain flag clear. | `[code]` approved | target |
| FR-6.3 | Must | Each reading's payload shall be exactly `{"value":<number with two decimals>,"unit":"<unit>"}`. | `[code]` approved | host |
| FR-6.4 | Must | The `rssi` reading shall carry unit `dBm`, `heap` unit `B`, and `uptime` unit `s`. | `[code]` approved | target |
| FR-6.5 | Must | On MQTT connect the DUT shall publish the status document to `smart_home/devices/<device_id>/status` with QoS 1 and the retain flag set. | `[code]` approved | target |
| FR-6.6 | Must | The status document shall contain the keys `status`, `device_type`, `name`, `firmware_version`, `ip`, `room`, `uptime_s`, `heap` and `rssi`. | `[code]` approved | host |
| FR-6.7 | Must | The status document's `status` field shall be `online` and its `device_type` field shall be `sensor_node`. | `[code]` approved | host |
| FR-6.8 | Must | The DUT shall re-publish the retained status document after every 10 telemetry cycles. | `[code]` approved | bench |
| FR-6.9 | Must | On `{"command":"get_status"}` the DUT shall re-publish the status document to the retained status topic. | `[code]` approved | target |
| FR-6.10 | Must | On `{"command":"reboot"}` the DUT shall publish the acknowledgement before initiating the restart. | `[code]` approved | bench |
| FR-6.11 | Must | On `{"command":"reboot"}` the DUT shall restart within 1 s of publishing the acknowledgement. | `[code]` approved | bench |
| FR-6.12 | Must | On `{"command":"ota"}`, when the body carries a non-empty `url` or a stored `ota_url` exists, the DUT shall request an update for that URL. | `[code]` approved | target |
| FR-6.13 | Must | On `{"command":"ota"}` with no URL in the body and none stored, the DUT shall acknowledge with `status` = `error`. | `[code]` approved | target |
| FR-6.14 | Must | On an unrecognised command verb the DUT shall acknowledge with `status` = `error`. | `[code]` approved | target |
| FR-6.15 | Must | On a body that is not valid JSON, or that has no string `command` field, the DUT shall publish no response. | `[code]` approved | target |
| FR-6.16 | Must | While MQTT is disconnected the DUT shall publish no sensor reading. | `[code]` approved | bench |
| FR-6.17 | Must | Every acknowledgement shall be a JSON object carrying `command`, `status` and `device_id`. | `[code]` approved | host |
| FR-6.18 | Should | The interval between consecutive telemetry cycles shall be 30 s within ±1 s. | `[code]` detected-in-code | bench |
| FR-6.19 | Should | The DUT shall not miss an MQTT keepalive deadline of 30 s while handling a command burst. | `[derived]` approved | bench |
| FR-6.20 | May | OTA progress may be reported on the response topic at a finer granularity than one message per integer percent; acceptance is a human review confirming the progress stream is received. | `[code]` approved | bench |

### 6.5 Verification contracts

Full contracts (a wrong pass is plausible) for the command path:

```yaml
id: FR-6.10 / FR-6.11
verification:
  preconditions:
    - The DUT is in RUNNING with an active MQTT session.
    - A subscriber is attached to smart_home/devices/<device_id>/response before the stimulus.
  stimulus:
    - Publish {"command":"reboot"} to smart_home/devices/<device_id>/command at QoS 1.
    - Record the time the acknowledgement is received.
    - Observe the reset (uptime restarts; boot banner reappears).
  expected_observations:
    - An acknowledgement with "command":"reboot","status":"ok" arrives on the response topic.
    - The DUT restarts, and the restart begins after the acknowledgement was published.
  timing: restart within 1 s of the acknowledgement publish
  tolerance: ±0.3 s
  prohibited_outcomes:
    - The DUT restarts before the acknowledgement reaches the broker.
    - No acknowledgement is received and the DUT restarts anyway.
    - The DUT restarts into a state where the console REPL is unavailable.
  tier: bench
  evidence:
    - Broker-side log with publish timestamps for command, acknowledgement and the Last Will offline status.
    - Serial capture showing the reset and the boot banner.
    - Reset reason from `version` after the reboot.
  cleanup:
    - Re-provision if the reset lost any state; confirm the retained status returns to "online".
```

```yaml
id: FR-6.15
verification:
  preconditions:
    - The DUT is in RUNNING; a subscriber is attached to the response topic.
  stimulus:
    - Publish a body that is not JSON (`not json`).
    - Publish a JSON object with no `command` field (`{"foo":1}`).
    - Publish a JSON object whose `command` is not a string (`{"command":5}`).
  expected_observations:
    - The DUT logs a warning for each message.
    - No message appears on the response topic within 5 s of each publish.
  timing: 5 s observation window per stimulus
  tolerance: 0 s
  prohibited_outcomes:
    - Any acknowledgement is published for an invalid body.
    - The DUT restarts or drops the MQTT session.
  tier: target
  evidence:
    - Serial log lines naming the rejected payloads.
    - Response-topic capture showing no messages in the window.
  cleanup:
    - None; the DUT remains in RUNNING.
```

Compact contracts:

| ID | Precondition · stimulus | Expected observation | Must NOT happen | Tier |
|---|---|---|---|---|
| FR-6.1 | DUT in RUNNING; subscribe to `.../sensor/#` for 70 s. | At least two complete sets of `rssi`, `heap`, `uptime`; gaps ≈30 s ±1 s. | A set with a missing channel; a gap >31 s or <29 s | bench |
| FR-6.2 | Subscribe with `-v`; inspect topic, QoS and retain flag. | Three topics under `.../sensor/`; QoS 0; retain clear | QoS 1; retained message; a topic outside `.../sensor/` | target |
| FR-6.3 | Capture one reading payload. | Parses as JSON with exactly `value` (two decimals) and `unit` | Extra keys; a non-numeric `value`; unescaped unit | host |
| FR-6.4 | Capture `rssi`, `heap`, `uptime`. | Units `dBm`, `B`, `s` respectively | Any other unit string; empty unit | target |
| FR-6.5 | Connect the DUT to the broker; read the retained message. | Retained message on `.../status`; QoS 1 delivery; survive a broker restart | Non-retained status; QoS 0 status | target |
| FR-6.6 | Read one status document. | All nine keys present | A missing key; a null value | host |
| FR-6.7 | Read one status document. | `status` = `online`, `device_type` = `sensor_node` | Any other value | host |
| FR-6.8 | Subscribe to `.../status` for 320 s. | A second status publish at ≈300 s | More than one extra publish in the window; none at all | bench |
| FR-6.9 | DUT in RUNNING; publish `{"command":"get_status"}`; subscribe to the status topic. | A fresh retained status document (all nine keys of FR-6.6) is published within 5 s; the acknowledgement is covered by FR-6.17 | No re-publish; a status document published only on the response topic | target |
| FR-6.12 | Store no `ota_url`; publish `{"command":"ota","url":"https://…/fw.bin"}`. | Ack `status` = `ok`; OTA transfers | Ack before the URL is validated; transfer of the stored URL | target |
| FR-6.13 | Store no `ota_url`; publish `{"command":"ota"}`. | Ack `status` = `error` | Any transfer; ack `ok` | target |
| FR-6.14 | Publish `{"command":"fly"}`. | Ack `{"command":"fly","status":"error",...}` | A state change; an OTA download; a restart | target |
| FR-6.16 | Stop the broker for 60 s; restart; capture the response topic and sensor topics. | After recovery, new readings only; no burst of buffered readings | More than one reading per channel within 1 s of recovery | bench |
| FR-6.17 | Capture any acknowledgement. | JSON with exactly `command`, `status`, `device_id` | Unescaped verb; missing `device_id`; malformed JSON | host |
| FR-6.18 | Timestamp 10 consecutive telemetry cycles. | Mean 30 s, each within ±1 s | A cycle <29 s or >31 s | bench |
| FR-6.19 | Publish 20 commands in 5 s. | The MQTT session survives ≥60 s without a keepalive disconnect | A broker-side keepalive timeout; a dropped session | bench |
| FR-6.20 | Trigger an OTA; watch the response topic. | Progress messages are received and each integer-percent change is reported | No progress at all; a progress value outside 0–100 | bench |

### 6.6 Failure modes

- **JSON parse failure / missing verb:** warn and discard; no response (FR-6.15).
- **Unknown verb:** acknowledge with `error` so the sender learns the command
  was rejected (FR-6.14).
- **OTA already in progress:** the `ota` request returns false, the service sets
  `isOTAInProgress()`, and the acknowledgement is still `ok` (FR-11.3).
- **MQTT down:** all publication is skipped; nothing is queued (FR-6.16).

### 6.7 Constants

| Constant | Value | Source |
|---|---|---|
| Telemetry interval | 30000 ms | `[code]` `Application.cpp` |
| Status refresh period | 10 telemetry cycles (300 s) | `[code]` `Application.cpp` |
| Sensor payload precision | two decimals (`%.2f`) | `[code]` `MqttService.cpp` |
| Reboot acknowledgement lead | 200 ms before `esp_restart()` | `[code]` `Application.cpp` |
| Response payload buffer | 96 bytes for fixed acks | `[code]` `Application.cpp` |

---

# Part B — Interfaces (L1)

## 7. WiFi Station Link

### 7.1 Purpose and peer

`WifiService` owns the station link to the household access point: configure,
start, associate, detect loss, and re-associate. The peer is an 802.11 AP the
operator provisioned. The ESP-IDF WiFi driver underneath is L0.

### 7.2 Requirements

| ID | Pri | Requirement | Prov / status | Tier |
|---|---|---|---|---|
| FR-7.1 | Must | On `connect()` the DUT shall configure the station with the SSID and passphrase read from NVS. | `[code]` approved | target |
| FR-7.2 | Must | The DUT shall refuse to associate with an access point whose authentication mode is below WPA/WPA2-PSK. | `[code]` approved | bench |
| FR-7.3 | Must | On a station disconnect while the attempt count is below 10, the DUT shall call `esp_wifi_connect()` again no sooner than 5 s after the previous attempt. | `[code]` detected-in-code | bench |
| FR-7.4 | Must | On reaching 10 failed attempts, the DUT shall stop re-attempting association until the next reset. | `[code]` detected-in-code | bench |
| FR-7.5 | Must | On acquiring an IPv4 address, the DUT shall reset the reconnect attempt count to zero. | `[code]` detected-in-code | bench |
| FR-7.6 | Must | The DUT shall report its acquired IPv4 address as a dotted-quad string in the status document's `ip` field. | `[code]` approved | target |
| FR-7.7 | Must | The DUT shall accept and use an SSID of up to and including 32 characters. | `[derived]` approved — **currently unmeetable** (OD-5) | target |
| FR-7.9 | Must | On `connect()` the DUT shall start the WiFi driver with the configured station settings. | `[code]` approved | target |

### 7.3 Verification contracts

```yaml
id: FR-7.3 / FR-7.4
verification:
  preconditions:
    - The DUT is provisioned with an SSID that does not exist (or the AP is powered off).
    - Serial output is captured.
  stimulus:
    - Reset the DUT and capture serial output for 90 s.
  expected_observations:
    - Log lines showing reconnect attempts numbered 1 through 10, spaced ≈5 s.
    - After attempt 10, no further attempt line for the remaining capture window.
    - The DUT remains alive and the console REPL still responds to `help`.
  timing: 5 s ±0.5 s between attempts; attempts 1–10 within 60 s
  tolerance: ±0.5 s per interval
  prohibited_outcomes:
    - Attempts continue past 10 without a reset.
    - The DUT restarts or panics.
    - The console becomes unresponsive.
  tier: bench
  evidence:
    - Serial capture with attempt timestamps.
    - `version` output after the run showing reset reason.
  cleanup:
    - Restore a valid SSID or repower the AP.
```

| ID | Precondition · stimulus | Expected observation | Must NOT happen | Tier |
|---|---|---|---|---|
| FR-7.1 | Credentials in NVS; read the station configuration after boot. | The stored SSID and passphrase are the ones applied | Association with an SSID other than the stored one | target |
| FR-7.2 | Configure an open (no-security) AP with the provisioned SSID. | Association fails; the DUT keeps retrying per FR-7.3 | Successful association to an open AP | bench |
| FR-7.5 | DUT associates successfully, then drops the AP once and reconnects. | Attempt counter restarts from 1 after the successful association | The counter continuing from its pre-connect value | bench |
| FR-7.6 | DUT has an IPv4 address; read the retained status. | `ip` is the dotted-quad address the AP leased | A stale address; an empty field; a MAC address | target |
| FR-7.7 | Provision a 32-character SSID and reset. | Association succeeds and the status `ip` is populated | Truncation; failure to associate; a 31-character SSID stored | target |
| FR-7.9 | Credentials in NVS; reset. | The driver starts and the station begins association | A configured station that never starts | target |

### 7.4 Failure modes

- **No SSID stored:** `connect()` returns false; the application does not start
  the service (FR-5.9).
- **Association failures:** bounded retry per FR-7.3/FR-7.4; after exhaustion the
  DUT is console-operable but not on the network. Whether permanent abandonment
  is intended is OD-14.
- **Driver init failure:** logged as `esp_wifi_init failed`; not retried.

## 8. WiFi Provisioning Store

### 8.1 Purpose and peer

`WifiConfigInterface` persists the SSID and passphrase in the NVS namespace
`wifi_config` and exposes them as console commands. The console command surface
itself is §12.

### 8.2 Requirements

| ID | Pri | Requirement | Prov / status | Tier |
|---|---|---|---|---|
| FR-8.1 | Must | `wifi_set <ssid> <password>` shall store both values in NVS namespace `wifi_config` under keys `ssid` and `password` and commit them. | `[code]` approved | target |
| FR-8.2 | Must | Stored WiFi credentials shall survive a reset without re-entry. | `[code]` approved | target |
| FR-8.3 | Must | `wifi_status` shall report whether WiFi credentials are configured. | `[code]` approved | target |
| FR-8.4 | Must | The firmware image shall contain no built-in SSID or passphrase. | `[user]` approved | host |
| FR-8.5 | Must | After `wifi_clear`, `hasCredentials()` shall return false. | `[derived]` approved — **currently unmet** (OD-4) | target |
| FR-8.6 | Must | A DUT with no stored credentials shall present a usable console REPL and shall not retry association. | `[code]` approved | target |
| FR-8.9 | Must | The boot following a successful `wifi_clear` shall not attempt association. | `[derived]` approved — **currently unmet** (OD-4) | target |
| FR-8.8 | Must | `wifi_set`, `wifi_ssid` and `wifi_password` shall each return exit status 0 on success and 1 on failure. | `[code]` approved | target |

### 8.3 Verification contracts

```yaml
id: FR-8.5 / FR-8.9
verification:
  preconditions:
    - The DUT has previously stored a valid SSID and passphrase.
    - A valid AP is available on that SSID.
  stimulus:
    - Run `wifi_clear` on the console.
    - Reset the DUT.
    - Capture serial output for 60 s.
    - Run `wifi_status`.
  expected_observations:
    - No association attempt appears in the boot log.
    - The DUT stays in INIT state.
    - `wifi_status` reports not configured.
  timing: 60 s observation window
  tolerance: 0 s
  prohibited_outcomes:
    - An association attempt with an empty SSID.
    - `wifi_status` reporting the credentials as configured.
    - The DUT restarting.
  tier: target
  evidence:
    - Serial capture of the boot and of `wifi_status`.
    - NVS namespace dump showing the keys erased (not present with empty values).
  cleanup:
    - Re-provision the original credentials.
```

| ID | Precondition · stimulus | Expected observation | Must NOT happen | Tier |
|---|---|---|---|---|
| FR-8.1 | Fresh NVS; run `wifi_set Foo secret`. | Success message; `wifi_status` shows `Foo`; keys present after reset | Values lost on reboot | target |
| FR-8.2 | Store credentials; reset; capture boot. | Association attempted with the stored SSID | A provisioning prompt requiring re-entry | target |
| FR-8.3 | Run `wifi_status` with credentials stored. | SSID shown; passphrase shown as `[CONFIGURED]` | The literal passphrase | target |
| FR-8.4 | Build the image; search the binary and a flash dump. | No SSID/passphrase literal present | Any credential in the image | host |
| FR-8.6 | Erase all NVS; reset. | `esp32>` prompt appears; `help` lists commands; no association attempt | A boot loop; a blocked console | target |
| FR-8.8 | Invoke each command with valid and with missing arguments. | Exit 0 on success, 1 on failure; failure prints usage | Exit 0 on a rejected argument count | target |

### 8.4 Failure modes

- **NVS open failure:** `initialize()` returns false and the application aborts
  initialisation with an error log.
- **`wifi_clear` defect:** documented as unmet (FR-8.5, OD-4); the code must be
  changed to erase the keys.

## 9. MQTT Wire Interface

### 9.1 Purpose and peer

`MqttService` owns the MQTT session to the broker: connection parameters, the
topic layout Smart_Server's bridge parses, per-class QoS and retain flags, the
Last Will, and the reconnect loop. The esp-mqtt client underneath is L0.

### 9.2 Protocol and topic schema

Protocol: MQTT 3.1.1 over plain TCP (`MQTT_TRANSPORT_OVER_TCP`), default port
1883. Prefix: `smart_home/devices/<device_id>` — the `devices` segment is
load-bearing because `Smart_Server/app/services/mqtt_bridge.py` splits the topic
positionally (`parts[2]` = device id, `parts[3]` = type, `parts[4:]` = sensor
path).

| Topic | Dir | QoS | Retain | Payload |
|---|---|---|---|---|
| `smart_home/devices/<id>/status` | out | 1 | yes | Status document (FR-6.6); Last Will publishes `{"status":"offline"}` |
| `smart_home/devices/<id>/sensor/<channel>` | out | 0 | no | `{"value":n,"unit":"u"}` |
| `smart_home/devices/<id>/command` | in | 1 (subscribe) | no | `{"command":"<verb>", …}` |
| `smart_home/devices/<id>/response` | out | 1 | no | Acknowledgement; OTA progress |

### 9.3 Requirements

| ID | Pri | Requirement | Prov / status | Tier |
|---|---|---|---|---|
| FR-9.1 | Must | The DUT shall build every topic under the prefix `smart_home/devices/<device_id>`. | `[code]` approved | host |
| FR-9.2 | Must | After each successful connect the DUT shall subscribe to `<prefix>/command` at QoS 1. | `[code]` approved | target |
| FR-9.3 | Must | Status publications shall use QoS 1 and retain. | `[code]` approved | target |
| FR-9.4 | Must | Sensor publications shall use QoS 0 and not retain. | `[code]` approved | target |
| FR-9.5 | Must | Response publications shall use QoS 1 and not retain. | `[code]` approved | target |
| FR-9.6 | Must | The DUT shall register a Last Will on `<prefix>/status` with payload `{"status":"offline"}`, QoS 1 and retain. | `[code]` approved | bench |
| FR-9.7 | Must | The DUT shall set its MQTT keepalive to 30 s. | `[code]` approved | bench |
| FR-9.8 | Must | The DUT shall request a persistent session by disabling the clean session flag. | `[code]` approved | bench |
| FR-9.9 | Must | The DUT shall connect to the configured host and port over plain TCP with client id equal to `device_id`, presenting username and password only when a username is configured. | `[code]` approved | bench |
| FR-9.10 | Must | After a disconnect the DUT shall retry the connection with exponential backoff: first retry at 2 s, doubling, capped at 60 s, with up to ±10 % jitter. | `[code]` approved | bench |
| FR-9.11 | Must | On every successful connect the DUT shall reset the backoff to its initial 2 s value. | `[code]` approved | bench |
| FR-9.12 | Must | On losing WiFi the DUT shall disconnect the MQTT client so the broker publishes the Last Will without waiting for the keepalive. | `[code]` approved | bench |
| FR-9.13 | Must | After the broker returns, the DUT shall publish a valid telemetry reading within 30 s of broker availability. | `[derived]` approved | bench |
| FR-9.14 | Must | A publish whose payload exceeds the 2048-byte outbox shall not be transmitted. | `[code]` approved | target |
| FR-9.18 | Must | A rejected oversize publish shall be logged at warning level naming the topic. | `[code]` approved | target |
| FR-9.15 | Must | A command published while the DUT is rebooting shall be delivered to the DUT after it reconnects. | `[code]` derived from persistent session | bench |
| FR-9.16 | Must | The DUT shall not queue sensor readings for delivery after a disconnect. | `[code]` approved | bench |
| FR-9.17 | Must | Retries of a lost MQTT session shall follow the cadence of FR-9.10 and shall not be issued at a fixed interval. | `[code]` detected-in-code | bench |

### 9.4 Verification contracts

```yaml
id: FR-9.6
verification:
  preconditions:
    - The DUT is in RUNNING; a subscriber is attached to .../status.
  stimulus:
    - Remove power from the DUT without a clean MQTT disconnect.
    - Observe the broker for the Last Will.
  expected_observations:
    - The broker publishes {"status":"offline"} on .../status within 45 s.
    - The message is retained (a later subscriber receives it).
  timing: within 45 s of power removal
  tolerance: +5 s
  prohibited_outcomes:
    - No Last Will is published.
    - The offline document is published on a different topic.
    - The retained status remains "online" after 45 s.
  tier: bench
  evidence:
    - Broker log with the Last Will publish and its timestamp.
    - A fresh subscriber receiving the retained offline document.
  cleanup:
    - Repower the DUT and confirm the retained status returns to "online".
```

```yaml
id: FR-9.13
verification:
  preconditions:
    - The DUT is connected to WiFi with an active MQTT session publishing telemetry.
    - A subscriber is attached to .../sensor/#.
  stimulus:
    - Stop the broker.
    - Keep it unavailable for 10 s.
    - Restart the broker; timestamp the moment it accepts connections.
  expected_observations:
    - The DUT detects the loss and enters MQTT_CONNECTING.
    - WiFi remains associated throughout.
    - A new MQTT session is established.
    - A valid telemetry reading arrives within 30 s of broker availability.
  timing: 30 s from broker availability to first valid telemetry
  tolerance: ±5 s
  prohibited_outcomes:
    - The DUT restarts.
    - Manual reprovisioning is required.
    - A stale or previously buffered reading is accepted as recovery evidence.
    - Telemetry arrives on a topic outside .../sensor/.
  tier: bench
  evidence:
    - Broker stop and start timestamps.
    - First valid telemetry message after recovery.
    - Reset reason or boot-counter evidence that no restart occurred.
    - Serial log showing the transition and the backoff lines.
  cleanup:
    - Ensure the broker is running; restore the DUT to RUNNING.
```

```yaml
id: FR-9.10 / FR-9.11
verification:
  preconditions:
    - The DUT is provisioned and has associated with WiFi.
    - The broker is stopped; serial output is captured with timestamps.
  stimulus:
    - Keep the broker down for 120 s.
    - Restart the broker and capture 30 s more.
  expected_observations:
    - Reconnect attempt intervals grow approximately 2, 4, 8, 16, 32, 60, 60 s, each within ±10 % jitter.
    - After the broker returns, the first retry succeeds and the next disconnect (if induced) starts again from ≈2 s.
  timing: per-interval as listed; the whole escalation within 120 s
  tolerance: ±10 % per interval
  prohibited_outcomes:
    - A fixed 10 s retry cadence (the library default).
    - Retries faster than 1.8 s or slower than 66 s.
    - No retry at all.
  tier: bench
  evidence:
    - Serial capture of the `Reconnect in <n> ms` lines with timestamps.
    - Post-recovery log showing the backoff reset.
  cleanup:
    - Leave the broker running.
```

| ID | Precondition · stimulus | Expected observation | Must NOT happen | Tier |
|---|---|---|---|---|
| FR-9.1 | Read the constructed topics. | Prefix `smart_home/devices/<id>` on every topic | `smart_home/<id>` (the stale ROADMAP form); a missing `devices` segment | host |
| FR-9.2 | Connect and inspect broker subscriptions. | A QoS 1 subscription to `.../command` after every connect | Subscription only on the first connect | target |
| FR-9.3 | Inspect a status publish. | QoS 1, retained | QoS 0; non-retained | target |
| FR-9.4 | Inspect sensor publishes. | QoS 0, retain clear | QoS 1; retained | target |
| FR-9.5 | Inspect a response publish. | QoS 1, retain clear | Retained acknowledgement | target |
| FR-9.7 | Inspect the CONNECT packet or broker log. | Keepalive 30 s | Any other keepalive value | bench |
| FR-9.8 | Inspect the CONNECT packet's clean-session flag. | Clean session = 0 | Clean session = 1 | bench |
| FR-9.9 | Inspect CONNECT with and without an MQTT username provisioned. | Client id = device id; credentials present only when configured | Empty username sent when unconfigured | bench |
| FR-9.12 | Establish a session; disable the AP. | The broker sees a DISCONNECT (or the socket close) within 2 s | The broker waiting out 45 s before the Last Will | bench |
| FR-9.14 | Cause a publish larger than the outbox (bench helper). | Nothing transmitted | Partial payload transmitted; a truncated message on the topic | target |
| FR-9.18 | Cause an oversize publish. | One warning log line naming the topic | A silent drop; an error-level line for a rejected oversize payload | target |
| FR-9.15 | Publish `get_status` while the DUT is rebooting; wait for reconnect. | The command executes after reconnect and an acknowledgement is published | The command lost | bench |
| FR-9.16 | Disconnect the broker for 90 s; reconnect. | At most one reading per channel immediately after recovery (the normal cadence) | A burst of buffered readings | bench |
| FR-9.17 | Inspect reconnect timing with the broker down. | Cadence matches FR-9.10, not a fixed 10 s | Fixed 10 s retries | bench |

### 9.5 Failure modes and recovery

- **Broker unreachable:** MQTT_CONNECTING with exponential backoff; the DUT
  never restarts (FR-5.10) and never buffers readings (FR-9.16).
- **Oversize payload:** rejected by the client; logged; not retried.
- **WiFi loss:** active disconnect so the Last Will fires promptly (FR-9.12).

### 9.6 Constants

| Constant | Value | Source |
|---|---|---|
| Topic prefix | `smart_home/devices` | `[code]` `MqttService.h` |
| Default port | 1883 | `[code]` `MqttConfigInterface.cpp` |
| Keepalive | 30 s | `[code]` `MqttService.cpp` |
| Outbox / inbox buffer | 2048 / 2048 bytes | `[code]` `MqttService.cpp` |
| MQTT task stack / priority / core | 4096 B / 5 / 0 | `[code]` `MqttService.h` |
| Initial / maximum backoff | 2000 ms / 60000 ms | `[code]` `MqttService.h` |

## 10. MQTT Configuration Store

### 10.1 Purpose and peer

`MqttConfigInterface` persists broker coordinates and device identity in the NVS
namespace `mqtt_config` and exposes them as console commands (`mqtt_set`,
`mqtt_auth`, `mqtt_device`, `mqtt_ota_url`, `mqtt_status`, `mqtt_clear`).

### 10.2 Requirements

| ID | Pri | Requirement | Prov / status | Tier |
|---|---|---|---|---|
| FR-10.1 | Must | `mqtt_set <host> [port]` shall store the host and port, defaulting the port to 1883 when absent or zero. | `[code]` approved | target |
| FR-10.2 | Must | `mqtt_auth <user> <password>` shall store both values; an absent username shall mean anonymous. | `[code]` approved | target |
| FR-10.3 | Must | `mqtt_device <id> <name> [room]` shall reject an id that is empty, longer than 24 characters, or contains a character outside `[a-z0-9_-]`, printing a rejection message and returning exit status 1. | `[code]` approved | host |
| FR-10.4 | Must | When no `device_id` is stored, the DUT shall derive `shnode-<hex of MAC bytes 3-5>` from the factory WiFi station MAC and persist it. | `[code]` approved | target |
| FR-10.5 | Must | When no name is stored, the DUT shall default it to `Smart Home <device_id>`. | `[code]` approved | host |
| FR-10.6 | Must | `mqtt_clear` shall erase the NVS keys so `hasConfig()` returns false. | `[code]` approved | target |
| FR-10.7 | Must | `mqtt_status` shall print host, port, device id, name, room and OTA URL, and shall print the password only as a presence marker. | `[code]` approved | target |
| FR-10.8 | Must | `MqttService::initialize()` shall refuse to initialise when the host or the device id is empty. | `[code]` approved | target |
| FR-10.9 | Must | Each stored string field shall have capacity for its maximum length plus one terminating NUL, with maxima: host 64, username 32, password 64, device id 24, name 32, room 24, OTA URL 192. | `[code]` approved | host |
| FR-10.10 | Must | Stored MQTT configuration shall survive a reset. | `[code]` approved | target |

### 10.3 Verification contracts

```yaml
id: FR-10.4
verification:
  preconditions:
    - NVS namespace mqtt_config has no devid key.
    - The board serial is captured.
  stimulus:
    - Reset the DUT twice.
    - Read the retained status document's implicit device id from its topic.
  expected_observations:
    - A device id of the form shnode-<6 hex digits> appears in the status topic.
    - The same id is used on both boots.
  timing: within the first status publish after each boot
  tolerance: 0
  prohibited_outcomes:
    - A different id on the second boot.
    - An id containing characters outside [a-z0-9_-].
    - A client id that differs from the topic segment.
  tier: target
  evidence:
    - Serial log line "Derived device id from MAC: shnode-……" on the first boot only.
    - Status topic captured on both boots.
  cleanup:
    - Optionally set a chosen device id with `mqtt_device`.
```

| ID | Precondition · stimulus | Expected observation | Must NOT happen | Tier |
|---|---|---|---|---|
| FR-10.1 | Fresh NVS; run `mqtt_set 10.0.0.5`. | Host stored; port defaults to 1883 | Port 0 stored | target |
| FR-10.2 | Run `mqtt_auth u p`. | Credentials stored; `mqtt_status` shows username `u` | Password printed | target |
| FR-10.3 | Run `mqtt_device BAD/ID Name`, then `mqtt_device ab Name`, then a 25-character id. | First and third rejected with exit 1; second accepted | A rejected id stored; a sanitised id stored silently | host |
| FR-10.5 | Set a device id with no name; reboot; read status. | `name` = `Smart Home <device_id>` | An empty name field | host |
| FR-10.6 | Store a full config; run `mqtt_clear`; reboot. | `mqtt_status` reports not provisioned; no MQTT connection attempted | Empty-string keys left behind so `hasConfig()` stays true | target |
| FR-10.7 | Run `mqtt_status` with credentials stored. | Host/port/id/name/room/ota URL shown; password shown as a marker | The literal password | target |
| FR-10.8 | Store a device id but no host; reset. | MQTT is not initialised; a provisioning hint is logged | A connection attempt to port 1883 on an empty host | target |
| FR-10.9 | Store a 64-character host, 24-character id, 32-character name; reboot. | All values round-trip without truncation | A truncated value; a NUL overrun | host |
| FR-10.10 | Store configuration; reset. | Every value persists | Values lost | target |

### 10.4 Failure modes

- **Buffer too small:** `nvs_get_str` fails and the field is treated as absent
  rather than truncated (a truncated broker hostname is worse than none).
- **Invalid id:** rejected; the existing id is kept.

## 11. OTA Update Receiver

### 11.1 Purpose and peer

`OTAService` receives a firmware image over HTTP(S) and writes it to the
inactive OTA slot. The peer is a static image server; the DUT pulls, it is never
pushed. Transport is `esp_https_ota` (L0).

### 11.2 Requirements

| ID | Pri | Requirement | Prov / status | Tier |
|---|---|---|---|---|
| FR-11.1 | Must | `requestUpdate(url)` shall set the pending flag and return before any network transfer begins. | `[code]` approved | target |
| FR-11.2 | Must | The OTA task shall begin the transfer within 1 s of the pending flag being set. | `[code]` approved | target |
| FR-11.3 | Must | A second update request while one is in progress shall be rejected and shall not start a second transfer. | `[code]` approved | target |
| FR-11.4 | Must | For an `https://` URL the DUT shall verify the server certificate against the CA bundle compiled into the image. | `[code]` approved | bench |
| FR-11.5 | Must | A production image shall refuse a plain `http://` OTA URL before any transfer starts. | `[user]` approved | target |
| FR-11.6 | Must | An image built with the OTA-test overlay shall accept a plain `http://` OTA URL. | `[code]` approved | bench |
| FR-11.7 | Must | The DUT shall publish one progress message on the response topic each time the integer percentage of bytes read changes. | `[code]` approved | target |
| FR-11.8 | Must | On a successful transfer the DUT shall finish the update, publish progress 100, and restart within 2 s. | `[code]` approved | bench |
| FR-11.9 | Must | On a failed transfer the DUT shall abort the update and continue running the current image. | `[code]` approved | bench |
| FR-11.10 | Must | An update shall be written to the OTA slot that is not running. | `[code]` approved | bench |
| FR-11.11 | Must | The DUT shall not apply a total-transfer timeout to an OTA download. | `[code]` approved | bench |
| FR-11.12 | Must | When the `ota` command carries no `url`, the DUT shall use the stored `ota_url`. | `[code]` approved | target |
| NFR-11.13 | Must | An image that transfers successfully but fails at runtime shall **not** be rolled back automatically in the current build. | `[code]` approved negative requirement | bench |
| NFR-11.14 | Should | An OTA download shall not add more than 200 ms of latency to a `get_status` command response. | `[derived]` proposed | bench |
| FR-11.15 | Must | On a failed transfer the DUT shall report an OTA error through the error handler. | `[code]` approved | bench |

### 11.3 Verification contracts

```yaml
id: FR-11.5
verification:
  preconditions:
    - A production image is flashed (sdkconfig.defaults only, no OTA-test overlay).
    - A static server serves a valid image over http:// on the LAN.
  stimulus:
    - Publish {"command":"ota","url":"http://<lan-ip>:8070/smart_home.bin"}.
    - Capture serial output and the response topic for 30 s.
  expected_observations:
    - The acknowledgement reports status ok (the request was accepted) but the transfer is refused.
    - The log names plain-HTTP OTA as disabled and points at the OTA-test build.
    - The DUT keeps running the current image.
  timing: refusal logged within 5 s of the request
  tolerance: +1 s
  prohibited_outcomes:
    - Any bytes of the image are written to a slot.
    - The DUT requests, receives or installs the image over http.
    - The DUT restarts.
  tier: target
  evidence:
    - Serial log line naming the refusal.
    - Server access log showing no GET for the image.
    - `version` output before and after showing the unchanged firmware version.
  cleanup:
    - None; the DUT remains on the current image.
```

```yaml
id: FR-11.8
verification:
  preconditions:
    - An OTA-test image is running; the served image differs in firmware_version.
    - The response topic and serial output are captured.
  stimulus:
    - Trigger the update with a valid http:// URL.
    - Wait for the transfer to complete.
  expected_observations:
    - Progress messages reach 100 on the response topic.
    - The DUT restarts within 2 s of the 100 % message.
    - After the reboot, `version` reports the new firmware version.
  timing: restart within 2 s of progress 100; new version visible within 30 s of reboot
  tolerance: +0.5 s
  prohibited_outcomes:
    - The DUT restarts into the old image.
    - The version string is unchanged after the reboot.
    - The DUT reboots before progress 100 is published.
  tier: bench
  evidence:
    - Full progress stream from the response topic.
    - Serial capture of the finish and restart.
    - `version` output before and after, with differing version strings.
  cleanup:
    - Reflash the production image, or trigger an update back to the baseline.
```

```yaml
id: FR-11.9 / FR-11.15
verification:
  preconditions:
    - The DUT is in RUNNING on a known firmware version.
    - The server returns an image whose transfer is interrupted (e.g. truncated body).
  stimulus:
    - Trigger the update.
    - Wait 60 s and read `version` and the retained status document.
  expected_observations:
    - The DUT logs an OTA transfer failure and reports an OTA error.
    - The running firmware version is unchanged.
    - The DUT remains in RUNNING and answers `get_status`.
  timing: failure logged within 60 s; DUT responsive throughout
  tolerance: 0
  prohibited_outcomes:
    - The DUT restarts into a partially written image.
    - The DUT stops answering MQTT.
    - The failed update is reported as success.
  tier: bench
  evidence:
    - Serial log of the abort and error report.
    - `version` output identical before and after.
    - Response to a post-failure `get_status`.
  cleanup:
    - Confirm the active slot is unchanged and the DUT is in RUNNING.
```

Compact contracts:

| ID | Precondition · stimulus | Expected observation | Must NOT happen | Tier |
|---|---|---|---|---|
| FR-11.1 | Publish an `ota` command with a slow server. | The acknowledgement arrives before any GET reaches the server | The MQTT task blocked until the transfer completes | target |
| FR-11.2 | Trigger an update; timestamp the request and the first GET. | Transfer begins within 1 s | A delay >1 s | target |
| FR-11.3 | Trigger two updates 100 ms apart. | The first proceeds; the second is rejected | Two concurrent transfers; a corrupted slot | target |
| FR-11.4 | Serve over HTTPS with a valid chain, then with an untrusted one. | Valid chain succeeds; untrusted chain is refused with a trust error | An untrusted certificate accepted | bench |
| FR-11.6 | Flash the OTA-test build; serve over http. | Transfer proceeds and completes | Refusal by the production check | bench |
| FR-11.7 | Trigger an update; capture the response topic. | Monotonic integer percentages ending at 100 | A decreasing value; a value >100; no messages | target |
| FR-11.10 | Note the running slot; perform an update. | Write targets the inactive slot | The running slot modified | bench |
| FR-11.11 | Serve a 4 MB image at ≤50 KB/s. | The transfer completes without a total-time abort | An abort caused by a wall-clock cap | bench |
| FR-11.12 | Store `ota_url`; publish `{"command":"ota"}` with no url. | The stored URL is used | No transfer; a different URL | target |
| NFR-11.13 | Flash an image that panics at boot over OTA. | The DUT remains on the failing image across ≥3 resets | An automatic rollback to the previous slot | bench |
| NFR-11.14 | Measure `get_status` round-trip with and without a concurrent download. | Added latency ≤200 ms | A response delayed beyond 200 ms | bench |
| FR-11.15 | Serve a corrupt/truncated image; watch the serial log and the category counters. | An OTA-category error is reported | A silent failure; an error attributed to another category | bench |

### 11.4 Failure modes

- **TLS trust failure:** logged with the trust explanation; the DUT keeps
  running the current image. The most common cause is an unset clock (OD-9).
- **Transfer failure:** `reportProgress(0)`, error reported, current image
  retained (FR-11.9).
- **No rollback:** a bad image persists; physical recovery is required
  (NFR-11.13, OD-11).

### 11.5 Constants

| Constant | Value | Source |
|---|---|---|
| OTA task stack / priority / core | 8192 B / 6 / 0 | `[code]` `OTAService.h` |
| OTA polling period | 500 ms | `[code]` `OTAService.cpp` |
| HTTP timeout | 0 (no total cap) | `[code]` `OTAService.cpp` |
| Restart delay after finish | 1000 ms | `[code]` `OTAService.cpp` |
| `partial_http_download` | false | `[code]` `OTAService.cpp` |
| OTA-test overlay | `sdkconfig.ota-test.defaults` | `[code]` |

## 12. Serial Console Command Surface

### 12.1 Purpose and peer

The console is the only provisioning and local-diagnosis interface. The peer is
a human operator on UART0 (or a USB-serial bridge). The REPL is `esp_console`
(L0); the command registry in `main/core/console` and the handlers in the config
interfaces and `SystemCommands` are L1.

### 12.2 Requirements

| ID | Pri | Requirement | Prov / status | Tier |
|---|---|---|---|---|
| FR-12.1 | Must | The DUT shall present an interactive REPL on UART0 at 115200 baud with the prompt `esp32>`. | `[code]` approved | target |
| FR-12.2 | Must | The command line length shall be limited to 256 characters. | `[code]` approved | target |
| FR-12.3 | Must | The DUT shall register `wifi_set`, `wifi_ssid`, `wifi_password`, `wifi_status`, `wifi_clear`, `mqtt_set`, `mqtt_auth`, `mqtt_device`, `mqtt_ota_url`, `mqtt_status`, `mqtt_clear`, `reboot`, `version` and `help`. | `[code]` approved | target |
| FR-12.4 | Must | A command invoked with too few arguments shall print its usage line and return exit status 1. | `[code]` approved | target |
| FR-12.5 | Must | `help` shall list every registered command grouped by feature. | `[code]` approved | target |
| FR-12.6 | Must | `help <command>` shall print that command's help text, and shall print `No such command: <name>` and return exit status 1 for an unknown name. | `[code]` approved | target |
| FR-12.7 | Must | `version` shall print the firmware version, build date and time, IDF version, uptime in seconds and the reset reason. | `[code]` approved | target |
| FR-12.8 | Must | `reboot` shall print a message and restart the DUT. | `[code]` approved | target |
| FR-12.9 | Must | The REPL shall be available, and configuration commands shall succeed, with no WiFi credentials and no MQTT configuration stored. | `[code]` approved | target |
| FR-12.10 | Must | Each configuration command that only takes effect on reboot shall say so in its confirmation message. | `[code]` approved | target |
| FR-12.11 | Must | Configuration commands shall return exit status 0 on success and 1 on failure. | `[code]` approved | target |

### 12.3 Verification contracts

| ID | Precondition · stimulus | Expected observation | Must NOT happen | Tier |
|---|---|---|---|---|
| FR-12.1 | Open UART0 at 115200 8N1 after reset. | `esp32>` prompt; typed characters echo; UP recalls history | No prompt; a different baud rate | target |
| FR-12.2 | Paste a 300-character line. | Input is truncated to 256 characters; the DUT does not fault | A buffer overrun; a crash | target |
| FR-12.3 | Run `help` at the prompt. | Every listed command appears | A missing command; a command that returns "unknown" | target |
| FR-12.4 | Run `mqtt_set` with no arguments. | Usage line printed; exit status 1 | A silent no-op; exit status 0 | target |
| FR-12.5 | Run `help`. | Commands appear under the feature headings `wifi`, `mqtt`, `system` | A flat alphabetical list | target |
| FR-12.6 | Run `help mqtt_set`, then `help nonsense`. | Help text for `mqtt_set`; `No such command: nonsense` and exit 1 | Exit 0 for an unknown command | target |
| FR-12.7 | Run `version`. | Five labelled fields present; reset reason matches the last reset | A missing field; `(no app description)` on a valid image | target |
| FR-12.8 | Run `reboot`. | `Rebooting...` then a restart with a fresh boot banner | A hang with no restart | target |
| FR-12.9 | Erase NVS; reset; run `wifi_set Foo bar` and `mqtt_set 10.0.0.5`. | Both succeed; console remains interactive | Commands blocked until WiFi/MQTT is up | target |
| FR-12.10 | Run `wifi_set`, `mqtt_set`, `mqtt_device`. | Each confirmation mentions restarting to apply | A message implying the change is already live | target |
| FR-12.11 | Cause a store failure (e.g. NVS full). | Confirmation reports failure; exit status 1 | Exit status 0 on failure | target |

### 12.4 Failure modes

- **A command fails to register:** logged; the rest of the boot proceeds
  (registration is not fatal).
- **A feature cannot be registered:** the grouping table still lists it, so the
  listing reflects intent; the log names the failure.

### 12.5 Constants

| Constant | Value | Source |
|---|---|---|
| Prompt | `esp32>` | `[code]` `Application.cpp` |
| Max command line | 256 chars | `[code]` `Application.cpp` |
| UART0 baud | 115200 | `[code]` `ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT` |
| Maximum registered features | 16 | `[code]` `Console.h` |

---

# Part C — Foundation / Transport (L0)

## 13. Platform Foundation and Transport Clients

### 13.1 Purpose — what we configure vs. what the platform owns

Everything in this chapter is ESP-IDF or a bundled component. We configure it;
we do not implement it and we do not test it directly. Its behaviour is
exercised **transitively** through the L1 and L2 requirements above — a
keepalive that does not fire shows up as a failed FR-9.7 contract, not as a
platform test of its own.

| Component | We configure | The component owns |
|---|---|---|
| FreeRTOS / ESP-IDF | Task stacks, priorities, core pinning, main stack size | Scheduling, heap, panic handling |
| NVS flash | Namespace and key layout, commit calls, erase-and-retry on corruption | Wear levelling, encoding, power-loss safety |
| `esp_netif` / `esp_event` | Default station netif, default event loop | DHCP, IPv4, event dispatch |
| esp-mqtt client | Broker address, credentials, keepalive, buffers, Last Will, QoS per publish, auto-reconnect off | MQTT 3.1.1 framing, keepalive emission, transport |
| `esp_https_ota` + esp-tls | URL, crt bundle attachment, no total timeout, `partial_http_download` off | TLS handshake, image validation, slot write |
| `esp_console` | Prompt, history, per-command registration | Line editing, tokenisation, dispatch |
| TWDT | 30 s timeout (via Kconfig) | Per-task subscription and reset semantics |
| cJSON | — | JSON parsing and printing |

### 13.2 Requirements

| ID | Pri | Requirement | Prov / status | Tier |
|---|---|---|---|---|
| NFR-13.1 | Must | The firmware shall build with ESP-IDF v5.5.1 for the `esp32s3` target using the committed `sdkconfig.defaults` and `partitions.csv`. | `[user]` approved | host |
| NFR-13.2 | Must | On `ESP_ERR_NVS_NO_FREE_PAGES` or `ESP_ERR_NVS_NEW_VERSION_FOUND` during NVS init, the DUT shall erase the NVS partition and initialise again. | `[code]` approved | target |
| NFR-13.3 | Must | The DUT shall run the task watchdog with a 30 s timeout and shall subscribe and feed it from the MQTT and OTA tasks. | `[code]` approved | target |
| NFR-13.4 | Must | The main task stack shall be 8192 bytes. | `[code]` approved | host |
| NFR-13.5 | Must | Every response body and status document shall parse as JSON, and a command verb echoed into a response shall round-trip unchanged. | `[code]` approved | host |
| NFR-13.6 | Must | The DUT shall create its service tasks with the stacks, priorities and cores recorded in Appendix F. | `[code]` approved constraint | review |
| NFR-13.7 | Must | The DUT shall reach the console prompt on every boot, including a boot with no WiFi or MQTT configuration and a boot after NVS erasure. | `[code]` approved | target |
| NFR-13.8 | Must | The DUT shall allocate FreeRTOS queues and task stacks from internal SRAM, not from PSRAM. | `[code]` approved constraint | review |
| NFR-13.9 | May | Additional tasks may be pinned to core 1 once there is work for them; acceptance is a human review confirming each core-1 task is named and documented in §2.1. | `[code]` approved | review |

### 13.3 Verification contracts

| ID | Precondition · stimulus | Expected observation | Must NOT happen | Tier |
|---|---|---|---|---|
| NFR-13.1 | Clean tree; run `./make.sh build`. | Build succeeds; generated `sdkconfig` shows target `esp32s3`, IDF v5.5.1 | A build for any other target; a configure error | host |
| NFR-13.2 | Corrupt the NVS partition (fill it, or raise its version); reset. | The DUT logs the erase-and-retry and boots to the console | A boot loop; an abort with no console | target |
| NFR-13.3 | Block the MQTT task (bench-injected) for 35 s. | The TWDT fires and the DUT restarts with reset reason `task watchdog` | An infinite hang with no reset | target |
| NFR-13.4 | Inspect the generated config. | `CONFIG_ESP_MAIN_TASK_STACK_SIZE=8192` | Any other value | host |
| NFR-13.5 | Send a command whose verb contains a JSON metacharacter. | The acknowledgement parses as JSON and round-trips the verb | Malformed JSON; an unescaped quote | host |
| NFR-13.6 | Review the task tables in §2.1 and Appendix F against the task creation calls. | Stacks, priorities and cores as recorded | An unpinned service task; an undocumented mismatch | review |
| NFR-13.7 | Erase NVS; reset; capture boot. | `esp32>` prompt appears; `help` responds | A hang before the prompt | target |
| NFR-13.8 | Review every queue and task-stack allocation capability against the internal-SRAM rule. | Internal SRAM only | A queue or task stack allocated from PSRAM | review |
| NFR-13.9 | Review §2.1 against the task list. | Each core-1 task is named with its responsibility | An undocumented core-1 task | review |

### 13.4 Failure and recovery

- **NVS corruption:** erase-and-retry (NFR-13.2); configuration is lost and must
  be re-provisioned. This is the only automatic recovery of persistent state.
- **Panic:** the default panic handler resets the DUT; the reset reason is
  visible through the `version` command.
- **Watchdog:** a task that fails to feed for 30 s triggers a reset with reset
  reason `task watchdog`.

---

# Part D — Cross-cutting Concerns

## 14. Device Identity

### 14.1 Purpose

Identity is what makes a node addressable and stable across reboots and
updates. It is both an MQTT topic segment and the MQTT client id, so it must
match a strict character class.

### 14.2 Requirements

| ID | Pri | Requirement | Prov / status | Tier |
|---|---|---|---|---|
| FR-14.1 | Must | The device id shall match `[a-z0-9_-]{1,24}`. | `[code]` approved | host |
| FR-14.2 | Must | When no device id is stored, the DUT shall derive `shnode-<six lowercase hex digits from MAC bytes 3-5>` and persist it before the first publish. | `[code]` approved | target |
| FR-14.3 | Must | The DUT shall use the device id as the MQTT client id and as the topic segment in every published topic. | `[code]` approved | host |
| FR-14.4 | Must | The device id shall be unchanged across a reboot and across an OTA update. | `[code]` approved | bench |
| FR-14.5 | Must | Two DUTs with unset device ids and different factory MACs shall derive different device ids. | `[derived]` approved | target |

### 14.3 Verification contracts

| ID | Precondition · stimulus | Expected observation | Must NOT happen | Tier |
|---|---|---|---|---|
| FR-14.1 | Feed candidate ids to the validator (host test). | `ab_1-x` accepted; `Ab`, `a/b`, `` and 25 characters rejected | A rejected id accepted; a valid id rejected | host |
| FR-14.2 | Erase NVS; reset twice. | The same `shnode-<hex>` id on both boots; a derivation log line on the first | A new id per boot; an id that is not persisted | target |
| FR-14.3 | Capture a status publish and the CONNECT packet. | Client id equals the topic's `<device_id>` segment | A different client id; an extra topic segment | host |
| FR-14.4 | Record the id; reboot; perform an OTA update. | The id is identical before and after both | A changed id | bench |
| FR-14.5 | Boot two boards with unset ids. | The two derived ids differ | Two boards deriving the same id | target |

### 14.4 Failure modes

- **No MAC available:** the id stays empty and MQTT initialisation refuses
  (FR-10.8); the DUT remains console-operable.
- **Operator sets a duplicate id:** two nodes share a topic namespace. Not
  prevented by the firmware; noted as an operational hazard in the manual.

## 15. Configuration Catalog

### 15.1 Configuration surface

| Field | Type | Default | Valid range | Persistence | Sensitivity | Interface | Validation | Change effect | Reset behaviour |
|---|---|---|---|---|---|---|---|---|---|
| `wifi_config/ssid` | string | none | 1–32 chars | NVS | personal | console `wifi_set`/`wifi_ssid` | non-empty | next boot | cleared by `wifi_clear` (see FR-8.5) |
| `wifi_config/password` | string | none | 0–64 chars | NVS | secret | console `wifi_set`/`wifi_password` | any | next boot | cleared by `wifi_clear` |
| `mqtt_config/host` | string | none | 1–64 chars | NVS | operational | console `mqtt_set` | non-empty | next boot | erased by `mqtt_clear` |
| `mqtt_config/port` | u16 | 1883 | 1–65535 | NVS | operational | console `mqtt_set` | non-zero | next boot | erased by `mqtt_clear` |
| `mqtt_config/user` | string | none (anonymous) | 0–32 chars | NVS | secret | console `mqtt_auth` | any | next boot | erased by `mqtt_clear` |
| `mqtt_config/pass` | string | none | 0–64 chars | NVS | secret | console `mqtt_auth` | any | next boot | erased by `mqtt_clear` |
| `mqtt_config/devid` | string | derived from MAC | `[a-z0-9_-]{1,24}` | NVS | operational | console `mqtt_device`, auto-derived | FR-14.1 | next boot | erased by `mqtt_clear`; re-derived |
| `mqtt_config/name` | string | `Smart Home <device_id>` | 0–32 chars | NVS | public | console `mqtt_device` | any | next boot | erased by `mqtt_clear` |
| `mqtt_config/room` | string | empty | 0–24 chars | NVS | public | console `mqtt_device` | any | next boot | erased by `mqtt_clear` |
| `mqtt_config/ota_url` | string | none | 0–192 chars | NVS | operational | console `mqtt_ota_url` | any | next command | erased by `mqtt_clear` |
| Telemetry cadence | build constant | 30000 ms | — | image | — | build-time | — | after reflash | n/a |
| Broker keepalive | build constant | 30 s | — | image | — | build-time | — | after reflash | n/a |
| Console prompt | build constant | `esp32>` | — | image | — | build-time | — | after reflash | n/a |

There is no network- or file-based configuration interface; every runtime field
is set over the serial console.

### 15.2 Requirements

| ID | Pri | Requirement | Prov / status | Tier |
|---|---|---|---|---|
| NFR-15.1 | Must | Every runtime-configurable value shall be stored in NVS; none shall be compiled into the image. | `[user]` approved | host |
| NFR-15.2 | Must | A stored configuration change shall take effect only after a reset, except `ota_url`, which shall be read at the time an `ota` command without a URL is handled. | `[code]` approved | target |
| NFR-15.3 | Must | `mqtt_clear` shall restore every `mqtt_config` field to its documented default, including re-derivation of the device id. | `[code]` approved | target |
| NFR-15.4 | Must | A value that exceeds its field's valid range or fails its validation rule shall be rejected and the previously stored value retained. | `[code]` approved | host |
| NFR-15.5 | Must | A sensitive field (WiFi passphrase, MQTT password) shall never be displayed by any console command, written to the serial log, or published on any MQTT topic. | `[code]` approved | target |
| NFR-15.6 | Must | `wifi_clear` shall restore the WiFi fields to the not-provisioned state. | `[code]` approved — **currently unmet** (OD-4) | target |

### 15.3 Verification contracts

| ID | Precondition · stimulus | Expected observation | Must NOT happen | Tier |
|---|---|---|---|---|
| NFR-15.1 | Build the image; inspect it and a flash dump. | No credential or per-device value present | A compiled-in SSID/password/device id | host |
| NFR-15.2 | Change `mqtt_set`; before rebooting, read the retained status. | The old broker is still in use until reset | A live reconnect to the newly stored broker | target |
| NFR-15.3 | Store full MQTT config; run `mqtt_clear`; reset. | No MQTT connection attempted; the device id is re-derived; every field is at its default | Empty-string keys left behind so `hasConfig()` stays true | target |
| NFR-15.4 | Store a 25-character device id; then a valid id. | Rejected; the previously stored id still in use | A truncated or sanitised value stored | host |
| NFR-15.5 | Run every console command that prints configuration, set a distinctive secret, and complete a telemetry cycle. | Secrets appear only as presence markers; the literal string appears in no command output, log line or MQTT payload | The literal passphrase or password anywhere | target |
| NFR-15.6 | Store WiFi credentials; run `wifi_clear`; inspect the namespace. | The `ssid` and `password` keys are absent; `hasCredentials()` is false | The keys present with empty values | target |

## 16. Error Handling and Diagnostics

### 16.1 Purpose

`ErrorHandler` gives every failure one structured line and a per-category
counter, so a failure is attributable to a subsystem without reading the code.

### 16.2 Requirements

| ID | Pri | Requirement | Prov / status | Tier |
|---|---|---|---|---|
| FR-16.1 | Must | `reportError` shall emit one log line at ERROR level containing the category name, the numeric code and the formatted message. | `[code]` approved | target |
| FR-16.2 | Must | `reportError` shall increment both the total counter and the counter for the given category. | `[code]` approved | host |
| FR-16.3 | Must | An MQTT transport error event shall be reported in the `MQTT` category. | `[code]` approved | bench |
| FR-16.4 | Must | An OTA begin, transfer or finish failure shall be reported in the `OTA` category. | `[code]` approved | bench |
| NFR-16.5 | Must | Error counters shall be volatile and shall reset to zero on every boot. | `[code]` approved | target |
| NFR-16.6 | Should | A category index outside the six declared categories shall not corrupt memory or counts. | `[code]` approved | host |

**Proposal, not adopted (OD-3):** publish the per-category counts in the
retained status document. `ErrorHandler.h` asserts a failing node can be
diagnosed "from its status topic", which is not true today. This proposal is
recorded here and deliberately **not** written as a requirement.

### 16.3 Verification contracts

| ID | Precondition · stimulus | Expected observation | Must NOT happen | Tier |
|---|---|---|---|---|
| FR-16.1 | Force an MQTT transport error (stop the broker). | One `ErrorHandler: [MQTT] error -1: transport error` line | A silent failure; a line with no category | target |
| FR-16.2 | Trigger three errors in two categories (host harness). | Total 3; the two category counts sum to 3 | A count outside the category array written | host |
| FR-16.3 | Stop the broker. | The error is attributed to `MQTT` | Attributed to `SYSTEM` or `UNKNOWN` | bench |
| FR-16.4 | Serve a corrupt OTA image. | Failures attributed to `OTA` | Attributed to another category | bench |
| NFR-16.5 | Read counts, reboot, read again (host harness or a debug command). | Counts start at zero after reboot | Counts surviving a reboot | target |
| NFR-16.6 | Call `reportError` with an out-of-range category (host harness). | No memory corruption; counts unchanged for valid categories | An out-of-bounds array write | host |

### 16.4 Failure modes

- Counters are in RAM and are not persisted (NFR-16.5); a reboot destroys them.
- Counters are not exposed; see the proposal above.

## 17. Logging and Observability

### 17.1 Purpose

The serial log is the only diagnostic channel that does not depend on the
network, so it must carry enough to diagnose a node that never reaches the
broker.

### 17.2 Requirements

| ID | Pri | Requirement | Prov / status | Tier |
|---|---|---|---|---|
| NFR-17.1 | Must | The image shall be built with maximum log level INFO so that INFO-level statements are present in the binary. | `[code]` approved | host |
| NFR-17.2 | Must | On boot the DUT shall log a banner naming the firmware, the ESP-IDF version and the FreeRTOS version. | `[code]` approved | target |
| NFR-17.3 | Must | After initialisation the DUT shall log the main task stack high-water mark in bytes. | `[code]` approved | target |
| NFR-17.4 | Must | Every telemetry cycle the DUT shall log one heartbeat line naming the application state, whether WiFi is up, and whether MQTT is up. | `[code]` approved | target |
| NFR-17.5 | Must | Log output shall be written to UART0 at 115200 baud. | `[code]` approved | target |

### 17.3 Verification contracts

| ID | Precondition · stimulus | Expected observation | Must NOT happen | Tier |
|---|---|---|---|---|
| NFR-17.1 | Inspect the generated config and the binary. | `CONFIG_LOG_MAXIMUM_LEVEL=3`; the banner string is present in the image | A level that strips `ESP_LOGI` | host |
| NFR-17.2 | Reset; capture the first 20 lines. | Firmware name, IDF version and FreeRTOS version are each present | A silent boot | target |
| NFR-17.3 | Reset; find the headroom line. | A byte count is printed after initialisation | Division or a null dereference in that path | target |
| NFR-17.4 | Capture 70 s of a RUNNING node. | At least two heartbeat lines with state, wifi and mqtt fields | A heartbeat with a missing field | target |
| NFR-17.5 | Open UART0 at 115200. | Readable text | Garbled output | target |

### 17.4 Constants

| Constant | Value | Source |
|---|---|---|
| Max log level | INFO (3) | `[code]` `sdkconfig.defaults` |
| Console / log UART | UART0, 115200 8N1 | `[code]` |
| Heartbeat period | 30000 ms | `[code]` `Application.cpp` |

## 18. Security Profile

This profile is stated **before** any security requirement, and every security
requirement below derives from it. Where the profile does not justify a
protection, the accepted risk is recorded rather than a requirement nobody
implements.

### 18.1 Threat model

| Element | Statement |
|---|---|
| Threat actors | (1) A LAN peer who can reach the MQTT broker; (2) a person with physical access to UART0/USB or the flash; (3) a network-path attacker who can substitute an OTA image; (4) a compromised image server |
| Physical access | Assumed possible for an attacker with the device in hand. The serial console grants full provisioning, reboot and OTA control. Flash readout is possible because flash encryption and secure boot are **not** enabled |
| Trusted networks | The home LAN is trusted; the bench network is **not**. The MQTT broker is assumed reachable only from the LAN |
| Remote exposure | None. The firmware opens no inbound listener and contacts no cloud service |
| Confidentiality required | WiFi passphrase and MQTT password: yes, against network observers of the *console log*; **not** claimed against physical flash readout |
| Integrity required | Firmware image: yes, against network substitution, via HTTPS with CA verification. MQTT payloads: no — plaintext, unauthenticated at the application layer |
| Credential lifecycle | Provisioned once over the console, stored in NVS, cleared with `wifi_clear`/`mqtt_clear`; no expiry, rotation or lockout |
| Firmware authenticity | HTTPS server verification with the compiled CA bundle, and the image checksum validated by `esp_https_ota`. No code signing, no anti-rollback |
| Secure boot / flash encryption | **Not enabled.** Accepted risk: NVS secrets are readable with physical flash access |
| Recovery / factory reset | No dedicated factory-reset command; NVS is erased automatically only on corruption; `wifi_clear`/`mqtt_clear` are the manual reset surface |

### 18.2 Accepted risks

| ID | Accepted risk | Why accepted |
|---|---|---|
| R-1 | MQTT is plaintext TCP with no TLS; a LAN observer can read and inject messages. | Home LAN is trusted; TLS is Phase 7 work (OD-10) |
| R-2 | NVS credentials are readable from flash with physical access. | Flash encryption and secure boot are out of scope; physical access is already game over via the console |
| R-3 | No secure boot or signed images; an attacker who controls the HTTPS endpoint can serve a valid image. | The endpoint is operator-controlled and HTTPS-verified; code signing is out of scope |
| R-4 | No anti-rollback: an older, validly signed image can be installed. | Rollback behaviour is disabled (OD-11); no version floor is specified |
| R-5 | The broker runs `allow_anonymous true` on the bench; anyone on that network can publish commands. | Bench-only; §18.3 requires authentication off-bench |
| R-6 | Obfuscation is **never** treated as encryption anywhere in this specification. | — |

### 18.3 Requirements

Requirements derived from this profile that are owned by another chapter are
**referenced, not restated** — restating them would give one fact two homes:

| Security obligation | Owning requirement |
|---|---|
| Production images refuse plain-HTTP OTA | FR-11.5 |
| HTTPS OTA verifies the server against the compiled CA bundle | FR-11.4 |
| MQTT credentials are presented only when configured | FR-9.9 |
| No stored secret reaches a log line, a console listing or an MQTT topic | NFR-15.5 |
| The image contains no built-in SSID or passphrase | FR-8.4 |

Requirements owned by this chapter:

| ID | Pri | Requirement | Prov / status | Tier |
|---|---|---|---|---|
| NFR-18.3 | Must | The plain-HTTP OTA allowance shall be enabled only by building with the OTA-test overlay, never in `sdkconfig.defaults`. | `[user]` approved | host |
| NFR-18.4 | Must | The firmware shall implement no Matter, Thread, Google or other cloud protocol. | `[user]` approved | bench |
| NFR-18.10 | Must | The firmware shall open no inbound network listener. | `[derived]` approved | bench |
| NFR-18.6 | Should | Off-bench operation shall use an authenticating MQTT broker, so that an anonymous LAN peer cannot publish commands. | `[user]` approved | bench |
| NFR-18.9 | Must | No acceptance criterion in this FSD shall accept "encrypted or obfuscated" as satisfying a protection. | `[derived]` approved | review |

### 18.4 Verification contracts

| ID | Precondition · stimulus | Expected observation | Must NOT happen | Tier |
|---|---|---|---|---|
| NFR-18.3 | Grep `sdkconfig.defaults` and build with and without the overlay. | The symbol appears only in the overlay; the production image refuses http | The symbol in `sdkconfig.defaults`; http accepted by a production build | host |
| NFR-18.4 | Inspect the protocol set in the image and on the wire. | No Matter/Thread/Google symbols or traffic | Any such protocol implemented | bench |
| NFR-18.10 | Port-scan the DUT from the LAN. | No listening TCP or UDP port | Any inbound listener | bench |
| NFR-18.6 | Point the DUT at an authenticating broker. | The session is accepted with the configured credentials | A fallback to anonymous | bench |
| NFR-18.9 | Review every acceptance criterion in this FSD. | No criterion of the form "encrypted or obfuscated"; accepted risks are listed in §18.2 | A criterion that cannot fail | review |

## 19. Build, Image and Platform Constraints

These are the numbers the build reads. If one is missing, a toolchain default
chooses it — which is how a phase arrives at the hardware for an answer the spec
should have given.

### 19.1 Flash and partition layout

From `partitions.csv` (`[code]`):

| Name | Type | SubType | Offset | Size |
|---|---|---|---|---|
| `nvs` | data | nvs | `0x9000` | `0x6000` |
| `otadata` | data | ota | `0xf000` | `0x2000` |
| `phy_init` | data | phy | `0x11000` | `0x1000` |
| `ota_0` | app | ota_0 | `0x20000` | `0x400000` |
| `ota_1` | app | ota_1 | `0x420000` | `0x400000` |
| `storage` | data | fat | `0x820000` | `0x7E0000` |

Bootloader offset `0x0`; partition table offset `0x8000`.

### 19.2 Requirements

| ID | Pri | Requirement | Prov / status | Tier |
|---|---|---|---|---|
| NFR-19.1 | Must | The DUT shall have at least 16 MB of flash; the build shall select 16 MB. | `[user]` approved | host |
| NFR-19.2 | Must | The DUT shall have 8 MB of octal PSRAM; the build shall select octal mode at 80 MHz. | `[user]` approved | host |
| NFR-19.3 | Must | The bootloader shall be written at offset `0x0` and the partition table at `0x8000`. | `[code]` approved | host |
| NFR-19.4 | Must | The partition table shall contain `otadata`, `ota_0` and `ota_1` in addition to `nvs` and `phy_init`, with the offsets and sizes in §19.1. | `[code]` approved | host |
| NFR-19.5 | Must | The application image shall be no larger than `0x400000` bytes. | `[code]` approved | host |
| NFR-19.6 | Must | Signals used by the firmware shall be restricted to GPIO 0–25 and 38–48. | `[user]` approved | review |
| NFR-19.7 | Must | GPIO 0, 3, 45 and 46 shall not be used unless the design declares the strapping consequence. | `[user]` approved | review |
| NFR-19.8 | Must | The NVS partition shall be `0x6000` bytes. | `[code]` approved | host |
| NFR-19.9 | Should | The build gate shall verify target, flash size, PSRAM mode, log level, the three OTA partition labels, and that the image fits a slot. | `[code]` approved | host |

### 19.3 Verification contracts

| ID | Precondition · stimulus | Expected observation | Must NOT happen | Tier |
|---|---|---|---|---|
| NFR-19.1 | Inspect `sdkconfig.defaults` and the generated config. | Flash size 16 MB | Any other size | host |
| NFR-19.2 | Inspect the generated config. | `CONFIG_SPIRAM=y`, `CONFIG_SPIRAM_MODE_OCT=y`, 80 MHz | Quad mode; another speed | host |
| NFR-19.3 | Inspect `build/flash_args` and the merge-bin offsets. | Bootloader at `0x0`; partition table at `0x8000` | Bootloader at `0x1000` | host |
| NFR-19.4 | Parse the generated `partition-table.bin`. | All six labels with the §19.1 offsets and sizes | A missing `otadata`/`ota_0`/`ota_1`; wrong offset | host |
| NFR-19.5 | Measure `build/smart_home.bin`. | ≤ `0x400000`; the current build is 838 KB `[code]` | An image larger than a slot | host |
| NFR-19.6 | Review every declared pin against the budget. | No signal outside 0–25 / 38–48 | A signal on 26–37 | review |
| NFR-19.7 | Review declared strapping pins. | 0/3/45/46 unused, or the consequence is declared | An undeclared strapping-pin use | review |
| NFR-19.8 | Parse the generated partition table. | `nvs` size `0x6000` | A different size | host |
| NFR-19.9 | Run `./make.sh smoke` against a deliberately misconfigured build. | Every check fails loudly on the misconfiguration | A green run on a wrong image | host |

---

# Part E — Operations and Verification

## 20. Operational Procedures

This is a **reading path** through a node's life, not a second specification.
Each step points at the chapter that owns the behaviour; the step-by-step
commands live in the OPERATE plane.

| Stage | What happens | Where the behaviour is specified | Where the steps live |
|---|---|---|---|
| 1. Build the image | Compile for esp32s3 with the committed partition table; inspect the image | §19, §13 | `docs/getting-started.rst`, `docs/deployment.rst` |
| 2. Flash | Write bootloader, partition table, app and OTA data over USB | §19.1 | `docs/getting-started.rst` |
| 3. First boot | Banner, console prompt, no credentials → `INIT` | FR-5.9, NFR-13.7, §17 | `docs/UserDocumentation/User-Manual.md` ch. 3 |
| 4. Provision | `wifi_set`, `mqtt_set`, `mqtt_device` into NVS; reboot to apply | §8, §10, §12 | `docs/getting-started.rst` |
| 5. Operate | Associate, connect, publish status and telemetry, accept commands | §5, §6, §7, §9 | `docs/UserDocumentation/User-Manual.md` ch. 4 |
| 6. Reconfigure | Change any field, then reboot | §15 | `docs/UserDocumentation/User-Manual.md` ch. 5 |
| 7. Update | Trigger OTA over MQTT; the DUT writes the inactive slot and restarts | §11 | `docs/deployment.rst` |
| 8. Diagnose | Serial log, `version` reset reason, retained status, `mqtt_status` | §17, §16, §12 | `docs/UserDocumentation/User-Manual.md` ch. 7 |
| 9. Recover | Re-provision, erase retained topics, USB reflash when an image is bad | §11.4, §15 | `docs/UserDocumentation/User-Manual.md` ch. 8 |

## 21. Verification and Validation

### 21.0 Test Architecture

Test tiers are cost-ordered execution environments. Each behaviour is placed at
the lowest tier where its bug can manifest.

| Tier | Runs on | Speed | Catches |
|---|---|---|---|
| **host** | Dev machine, plain compiler, no ESP-IDF, no hardware | ms, every commit | Pure logic: topic formatting, JSON construction, device-id validation, range/bound predicates |
| **target** | One ESP32-S3 board, real ESP-IDF, USB or UART attached | seconds, pre-merge | NVS persistence, WiFi association, MQTT wire behaviour, console REPL, flash writes, RTOS behaviour |
| **bench** | Board + real peers (AP, broker, Smart_Server, image server) | minutes, pre-release | End-to-end recovery, reconnection, wall-clock timing, Last Will, OTA |

**Current tier availability: none.** No testbench Pi is available, no ESP32 is
connected, and no host test target is wired. Every requirement is therefore
unproven until Phase 1 (`/harness`) establishes the host tier and the bench
exists. Requirements whose tier is `bench` cannot be proven at all until the
bench is commissioned; they are listed in the gaps below and must not be
reported as passing.

**Layer → tier mapping** (mirrors §2.4):

| Layer | Tier treatment |
|---|---|
| L0 Foundation | Tested **transitively** through L1/L2 requirements — no tier of its own |
| L1 Interfaces | Pure core (topic/JSON/validation predicates) at **host**; wire and flow at **target**; peer interaction at **bench** |
| L2 Application logic | Decision functions at **host** where extracted; transitions and cadence at **target**/**bench** |

**Proposed host-tier seams (OD-13):** topic construction (`topicFor`), status
and acknowledgement JSON construction, sensor payload formatting, device-id
validation, config field-length predicates. None of these is extracted as a
free function today, so the host tier currently reaches nothing. Extracting them
is Phase 1 work.

**Component × tier coverage** is generated, never hand-maintained (see §21.2).

### 21.1 Acceptance tests

End-to-end scenarios that must pass before a phase is called done:

| ID | Scenario | Requirements exercised | Tier | Preconditions |
|---|---|---|---|---|
| AT-1 | Cold boot to console, provision, reboot, associate, connect, appear in Smart_Server | FR-5.1, FR-5.2, FR-5.9, FR-8.1, FR-10.1, FR-12.1, NFR-13.7 | bench | AP + broker + Smart_Server available |
| AT-2 | Telemetry and status observed end to end with no sensor hardware | FR-6.1–FR-6.8, FR-9.3, FR-9.4 | bench | AT-1 passed |
| AT-3 | Command round-trip: `get_status`, unknown verb, invalid JSON | FR-6.9, FR-6.14, FR-6.15, FR-6.17 | bench | AT-2 passed |
| AT-4 | Broker restart recovery without a reboot | FR-9.10, FR-9.11, FR-9.13, FR-5.4 | bench | AT-2 passed |
| AT-5 | AP loss and recovery without a reboot | FR-5.5, FR-5.10, FR-7.3, FR-7.5 | bench | AT-2 passed |
| AT-6 | Power removal fires the Last Will within 45 s | FR-9.6 | bench | AT-2 passed |
| AT-7 | OTA over plain HTTP on the bench image changes the running version | FR-11.2, FR-11.6–FR-11.8 | bench | OTA-test image; static server |
| AT-8 | OTA over HTTP is refused by a production image | FR-11.5, NFR-18.3 | target | Production image; static server |
| AT-9 | Failed OTA keeps the current image and the DUT responsive | FR-11.9, NFR-11.13 | bench | Truncating server |
| AT-10 | Reboot command acknowledges before restarting | FR-6.10, FR-6.11 | bench | AT-3 passed |
| AT-11 | Device identity survives reboot and OTA | FR-14.4, FR-14.2 | bench | AT-1 and AT-7 passed |
| AT-12 | Image and partition gate fails loudly on a misconfigured build | NFR-19.1–NFR-19.5, NFR-19.9 | host | Build tree |
| AT-13 | Console remains usable with no configuration at all | FR-12.9, FR-8.6, NFR-13.7 | target | Erased NVS |
| AT-14 | Two boards derive distinct identities | FR-14.5 | target | Two boards, unset ids |

No performance, load or scalability acceptance test is specified: the DUT has a
single client and no throughput target, and no value would be honest without a
measurement (OD-17).

### 21.2 Traceability

Traceability is **generated, never hand-filled**. This FSD carries the stable
requirement IDs and their verification contracts; the test plan (Phase 1,
`/harness`) carries the test entries that cite those IDs. A traceability tool
crosses requirement → component → tier with test linkage to emit:

- `tests/coverage-matrix.md` — covered/total per component × tier
- `tests/gaps.md` — requirements with no test, by category

**Neither file exists yet**; the plan and the tool are Phase 1 deliverables.
Until they exist, coverage cannot be computed and no requirement may be reported
as covered. This document deliberately contains **no** hand-maintained
Covered/GAP column.

**Gaps by category, as of this document:**

| Category | Gap |
|---|---|
| Specification | No boot deadline (OD-17); no rollback threshold (OD-11); no actuator/sensor requirements (Phases 3 and 5) |
| Verification | Every requirement: no test exists, and no tier is available |
| Implementation | FR-7.7 (32-char SSID), FR-8.5 / FR-8.9 / NFR-15.6 (`wifi_clear`), FR-5.7 (unreachable states — pending OD-1) |
| Evidence | No hardware has been run; only build-level smoke evidence exists (§1.6) |
| `pending` requirements | None — no requirement is marked `pending` in this document |
| `philosophical` requirements | FR-6.20 and NFR-13.9 (human-judgement acceptance) |

---

## Appendices

### Appendix A — MQTT topics, QoS and retain

| Topic | Dir | QoS | Retain | Payload shape |
|---|---|---|---|---|
| `smart_home/devices/<id>/status` | out | 1 | yes | See Appendix B |
| `smart_home/devices/<id>/sensor/rssi` | out | 0 | no | `{"value":-63.00,"unit":"dBm"}` |
| `smart_home/devices/<id>/sensor/heap` | out | 0 | no | `{"value":123456.00,"unit":"B"}` |
| `smart_home/devices/<id>/sensor/uptime` | out | 0 | no | `{"value":420.00,"unit":"s"}` |
| `smart_home/devices/<id>/command` | in | 1 | no | `{"command":"get_status"}` |
| `smart_home/devices/<id>/response` | out | 1 | no | See Appendix C |

### Appendix B — Status document

```json
{
  "status": "online",
  "device_type": "sensor_node",
  "name": "Smart Home shnode-01",
  "firmware_version": "<git describe>",
  "ip": "192.168.1.42",
  "room": "kitchen",
  "uptime_s": 420,
  "heap": 123456,
  "rssi": -63
}
```

The Last Will publishes `{"status":"offline"}` on the same topic. The document
carries no application state field (OD-2) and no error counters (OD-3).

### Appendix C — Acknowledgement bodies

| Command | Body |
|---|---|
| `get_status` | `{"command":"get_status","status":"ok","device_id":"<id>"}` |
| `reboot` | `{"command":"reboot","status":"ok","device_id":"<id>"}` then restart |
| `ota` | `{"command":"ota","status":"ok","device_id":"<id>"}`, then `{"command":"ota","status":"progress","percent":<n>}` |
| unknown | `{"command":"<verb>","status":"error","device_id":"<id>"}` |
| invalid body | *no response* |

### Appendix D — Console commands

| Feature | Command | Purpose |
|---|---|---|
| wifi | `wifi_set <ssid> <password>` | Store both credentials |
| wifi | `wifi_ssid <ssid>` | Store the SSID |
| wifi | `wifi_password <password>` | Store the passphrase |
| wifi | `wifi_status` | Show whether credentials are configured |
| wifi | `wifi_clear` | Clear credentials (FR-8.5 currently unmet) |
| mqtt | `mqtt_set <host> [port]` | Store broker coordinates |
| mqtt | `mqtt_auth <user> <password>` | Store broker credentials |
| mqtt | `mqtt_device <id> <name> [room]` | Store identity |
| mqtt | `mqtt_ota_url <url>` | Store a default OTA URL |
| mqtt | `mqtt_status` | Show MQTT configuration |
| mqtt | `mqtt_clear` | Erase MQTT configuration |
| system | `version` | Firmware, build, IDF, uptime, reset reason |
| system | `reboot` | Restart |
| console | `help [command]` | Grouped command listing |

### Appendix E — NVS layout

| Namespace | Key | Type | Max length / value |
|---|---|---|---|
| `wifi_config` | `ssid` | string | 31 chars in practice (OD-5); 32 intended |
| `wifi_config` | `password` | string | 64 chars |
| `mqtt_config` | `host` | string | 64 chars |
| `mqtt_config` | `port` | u16 | 1–65535 (default 1883) |
| `mqtt_config` | `user` | string | 32 chars |
| `mqtt_config` | `pass` | string | 64 chars |
| `mqtt_config` | `devid` | string | 24 chars, `[a-z0-9_-]` |
| `mqtt_config` | `name` | string | 32 chars |
| `mqtt_config` | `room` | string | 24 chars |
| `mqtt_config` | `ota_url` | string | 192 chars |

### Appendix F — Task, stack and buffer constants

| Item | Value | Source |
|---|---|---|
| Main task stack | 8192 B | `[code]` `sdkconfig.defaults` |
| WiFi task | 3072 B, prio 5, core 0; event queue 5 entries | `[code]` `WifiService.h` |
| MQTT task | 4096 B, prio 5, core 0 | `[code]` `MqttService.h` |
| OTA task | 8192 B, prio 6, core 0; poll 500 ms | `[code]` `OTAService.h` |
| MQTT buffers | 2048 B in, 2048 B out | `[code]` `MqttService.cpp` |
| TWDT timeout | 30 s | `[code]` `sdkconfig.defaults` |
| Max console line | 256 chars | `[code]` `Application.cpp` |

### Appendix G — Constants not yet chosen

| Value | Needed by | Status |
|---|---|---|
| Boot deadline, reset → first steady state | §3.2 exit criteria | Unknown; must be measured in Phase 0 (OD-17) |
| Rollback failure threshold (restarts before rollback) | NFR-11.13, Phase 7 | Unknown; must not be invented (OD-11) |
| Added-latency budget during OTA | NFR-11.14 | Proposed 200 ms, unaccepted |

---

## Related

- [`../../README.md`](../../README.md) — what the firmware does today
- [`../../ROADMAP.md`](../../ROADMAP.md) — phase plan and the hardware verification checklist
- [`../00-Overview.md`](../00-Overview.md) — the plane map and authority order
- [`../Method/AI-Workflow.md`](../Method/AI-Workflow.md) — how a change is made
- [`../UserDocumentation/User-Manual.md`](../UserDocumentation/User-Manual.md) — how to install and run a node
- [`../architecture.rst`](../architecture.rst) — the bound Sphinx architecture page
- `Smart_Server/app/services/mqtt_bridge.py` — the positional topic parser the topic schema must match

---

## Document lifecycle

```yaml
document_status: draft — awaiting user acceptance of the open decisions in §4.6
fsd_version: 0.1.0
repository: https://github.com/sprchuoi/smart_home_idf
baseline_commit: 1a62efd
applicable_firmware_version: build from baseline_commit (image 838 KB at the Phase 1 build)
author: define skill (Phase 0), for the repository owner
reviewers: pending
approval_status: not approved
created: 2026-09-26
last_updated: 2026-09-26
change_history:
  - version: 0.1.0
    date: 2026-09-26
    change: Initial FSD created from README.md, ROADMAP.md, main/, the build configuration and the bound docs/ set.
superseded_requirements: none (no prior FSD existed)
open_decisions: OD-1 … OD-17 (§4.6)
related_test_baseline: none — testing/test-plan.yaml is a Phase 1 (/harness) deliverable
```

