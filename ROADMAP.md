# Smart Home → Google Home — Roadmap & Progress

**Status as of 2026-09-16** · branch `phase1-esp32s3-retarget`

Goal: a DIY smart home controllable from Google Home, with sensors on ESP32 hardware and a
Raspberry Pi as the hub.

Legend: `[x]` done · `[~]` in progress · `[ ]` todo · `[!]` blocked · `?` needs a decision

---

## Progress at a glance

| Phase | Scope | Status |
|---|---|---|
| 0 | Prerequisites & hardware ground truth | `[ ]` not started |
| 1 | Retarget to ESP32-S3, clear dead weight | `[~]` **build + CI green; hardware verification outstanding** |
| 2 | MQTT + Home Assistant | `[~]` **firmware complete; no broker contacted yet** |
| 3 | Sensors | `[ ]` not started |
| 4 | Google Home via Matter bridge | `[ ]` not started |
| 5 | Actuators | `[ ]` not started |
| 6 | nRF5340 Thread sensor node | `[ ]` not started |
| 7 | OTA + hardening | `[ ]` not started |
| 8 | Voice (parked) | `[ ]` parked |

---

## Decisions taken

| Decision | Choice |
|---|---|
| Firmware | ESP-IDF v5.5.1 for ESP32-S3; Zephyr kept solely for the nRF5340 |
| Google Home path | Home Assistant + `RiDDiX/home-assistant-matter-hub` Matter bridge |
| nRF5340 | Thread sensor node via Home Assistant's own border router — later phase |
| Target board | ESP32-S3-DevKitC-1 **N16R8** — 16 MB flash, 8 MB octal PSRAM |

### Why this architecture

Google only needs to see a **Matter bridge**; how Home Assistant gets the data is irrelevant to
it. So the ESP32-S3 stays on plain MQTT (the one thing already partly working) and the Pi does
the bridging. Result: **$0 recurring**, nothing exposed publicly, no OAuth server, no cloud
project, no firmware rewrite onto ESP-Matter.

```
        Google Home app  ·  Nest speaker
                    │  Matter (local, QR pairing)
        ┌───────────▼─────────────────────────────┐
        │  Raspberry Pi                            │
        │    Mosquitto   ← MQTT broker             │
        │    Home Assistant                        │
        │      └─ HAMH add-on → Matter bridge      │
        └───────────┬──────────────────────────────┘
                    │  MQTT over WiFi 2.4 GHz
        ┌───────────▼──────────────────────────────┐
        │  ESP32-S3 nodes — sensors + actuators    │
        └──────────────────────────────────────────┘
```

---

## Phase 0 — Prerequisites

No code. Confirm ground truth before trusting the partition layout.

- [ ] Confirm board revision and actual flash/PSRAM (`esptool.py flash_id`). A wrong
      `CONFIG_SPIRAM_MODE_OCT` gives a **boot loop**, not a build error.
- [ ] Obtain **one Google Nest speaker/hub** (Nest Mini is enough). The phone app alone cannot
      do voice control.
- [ ] Confirm the Pi's OS, storage, and that it is on the same L2 subnet as the nodes.
- [ ] Router: **IPv6 on, IGMP snooping off, AP isolation off.** This is the #1 cause of Matter
      bridge "No Response" and it is a network problem, not software.
- [ ] Note the usable GPIO budget: 0-25 and 38-48 only. See *Verified findings* below.

**Exit criteria:** board flashed with any known-good firmware and serial logs readable.

---

## Phase 1 — Retarget to ESP32-S3 and clear dead weight

### Build & target — DONE

- [x] `sdkconfig.defaults` at repo root, retargeted to esp32s3 / 16 MB / octal PSRAM @ 80 MHz
- [x] `sdkconfig.defaults` un-ignored in `.gitignore` (a fresh clone previously could not
      reproduce the build)
- [x] `partitions.csv` rewritten: dual 4 MB OTA slots (previously **no OTA slot at all**)
- [x] `main/CMakeLists.txt`: dropped `.hpp` from `SRCS`, dropped the dead
      `set(SDKCONFIG_DEFAULTS ...)`
- [x] **Build verified green**: `smart_home.bin` 838 KB, 80 % of the app slot free
- [x] Logging un-broken: `LOG_MAXIMUM_LEVEL` 2 → 3, so `ESP_LOGI` compiles in at all

### Critical bugs fixed

- [x] **WiFi never started.** `WifiService::connect()` — which holds `esp_wifi_start()` — was
      called by nothing. The radio never came up and everything downstream was dead.
- [x] **`UartDriver::stop()` tore down the console** — called `uart_driver_delete(UART_NUM_0)`
      on the esp_console REPL's driver, then double-freed its event queue. Fired on every
      shutdown.
- [x] **EventBus 51.8 KB → 3.8 KB** and made value-semantic. See *Verified findings*.
- [x] Removed the commented-out service graveyard from `Application.cpp`; replaced with an
      honest "not yet enabled, and why" block.

### CI & tooling — DONE

> **Correction:** the first CI rewrite still failed, with `idf.py: command not found`.
> `espressif/esp-idf-ci-action` is a *container* action — it runs `docker run` and executes its
> `command` input inside that image. `idf.py` exists only in there, so the build must go through
> the action's `command` input; a plain `run: idf.py build` step runs on the runner host where
> IDF is not installed. Also found while fixing it: **cppcheck is not on the `ubuntu-latest`
> runner image** (checked against actions/runner-images), so that step would have failed next —
> it now installs cppcheck explicitly. The fix is based on reading the action's source, not yet
> confirmed by a green run.

- [x] CI de-lied. The 9 `\\` continuations (and several `\"`) are gone; the YAML parses and the
      steps actually run. Static analysis uses `continue-on-error` instead of `|| true`, so
      findings are visible rather than swallowed.
- [x] Build now goes through the container action's `command` input, which is the only place
      `idf.py` exists.
- [x] CI verifies the image itself (target, flash, PSRAM mode, log level, and the *generated*
      partition table) rather than just reporting a size — same assertions as `./make.sh smoke`,
      parsed from `partition-table.bin` so a bad CSV that still builds is caught.
- [x] Target corrected to `esp32s3`; `IDF_VERSION` was self-contradictory (v5.2 in env vs v5.5
      in the action input) — now v5.5.1 throughout.
- [x] QEMU job deleted, and removed from the `needs` of the report and release jobs. It could
      never pass: `qemu-system-xtensa` cannot emulate an S3, *and* it grepped for a log string
      that `LOG_MAXIMUM_LEVEL=WARN` had compiled out of the binary.
- [x] Docs build dropped from CI — `docs.yml` already does it and deploys to Pages.
- [x] Actions pinned forward (`checkout@v4`, `action-gh-release@v2`); old pins were Node 16.
- [x] `make.sh` target corrected; QEMU replaced by **`./make.sh smoke`**, which checks target,
      flash size, octal PSRAM, log level, and the presence of `otadata`/`ota_0`/`ota_1`.
      Verified these fail against the previous configuration.
- [x] `./make.sh ci` runs the smoke gate as a real failing step.
- [x] `run_static_analysis` creates `$TEST_DIR`; previously only `setup_environment` did, so
      `./make.sh test` on a fresh clone could not write its report.
- [x] **Credentials no longer baked in.** `main/Kconfig`'s `CONFIG_WIFI_SSID`/`_PASSWORD` are
      gone; provisioning is NVS-only via `wifi_set <ssid> <password>`.
- [x] `main/sdkconfig.defaults` (orphaned, held placeholder credentials) removed.
- [x] `Application_cfg.hpp`'s `idle_core_mask` type confusion documented and defused.

> `main/Kconfig` and `main/app/cfg/Application_cfg.hpp` were **emptied rather than deleted** —
> file removal needs explicit user confirmation. Both now contain only a tombstone explaining
> why they are empty. They are safe to delete.

### Still outstanding in Phase 1

- [x] `debug.sh` now resolves GDB from the configured target (`xtensa-esp32s3-elf-gdb`) instead
      of hardcoding `xtensa-esp32-elf-gdb`, which cannot debug an S3.
- [ ] **Hardware verification — nothing has run on a board yet.** See the checklist at the end.
      This is the gate on declaring Phase 1 finished.
- [ ] Host-side unit tests via IDF's `linux` preview target (`PREVIEW_TARGETS` in
      `tools/idf_py_actions/constants.py`). `smoke_test` covers build-level regressions but
      there is still no test for pure logic — HA discovery payloads, topic formatting, sensor
      conversion. A test that publishes an event with a short-lived payload and asserts the
      consumer sees the right bytes would lock in the EventBus fix.
- [ ] Decide the four **Open decisions** below (EventBus keep/delete is cheapest to settle now).

**Exit criteria:** board boots on S3, joins WiFi from NVS credentials, reports its IP, and the
boot heap reflects the EventBus saving.

---

## Phase 2 — MQTT and Home Assistant

### Firmware — DONE

- [x] **EventBus replaced with direct callbacks.** The topology is a DAG
      (`WiFi → MQTT → {publish, command → OTA}`) with no fan-out and one listener per edge.
      Callbacks also make the dangling-pointer bug structurally impossible, since nothing is
      queued. Note the bus had **zero consumers** — its drain points were called only from
      services that were never instantiated.
- [x] `session.last_will` set on `smart_home/<id>/availability`. Without it the discovery
      payload advertised an availability topic that was never published, so every entity
      would have read `unavailable` forever.
- [x] `disable_auto_reconnect = true` and an owned reconnect loop with exponential backoff and
      jitter — esp-mqtt's built-in reconnect is a fixed 10 s (`MQTT_RECON_DEFAULT_MS`) with
      no backoff.
- [x] QoS per topic class: **1 + retained** for availability/discovery/status (all must survive
      a broker restart); **0** for telemetry (a stale reading has no value, and QoS 1 telemetry
      during an outage just fills the outbox); **1** for commands, with a persistent session so
      a command sent while the node reboots is queued.
- [x] The do-nothing 5 s task is now a real reconnect supervisor.
- [x] Topic schema: `smart_home/<id>/{availability,status,<chan>/state,cmd/<target>}`.
- [x] `MqttConfigInterface`: broker, credentials, device id/name/room and OTA URL in NVS, with
      `mqtt_set` / `mqtt_auth` / `mqtt_device` / `mqtt_status` / `mqtt_clear`. Its `clear()`
      erases keys rather than writing `""` — the WiFi equivalent writes empty strings, which
      leaves the key present so `hasCredentials()` keeps returning true.
- [x] Device identity validated to `[a-z0-9_-]`; derived from MAC and persisted if unset.
- [x] Config buffers are `MAX_LEN + 1`. (`Wifi_cfg.hpp`'s `char ssid[32]` cannot hold a legal
      32-character SSID — still to fix.)
- [x] HA discovery per entity via cJSON, with a shared `device` block, republished on every
      connect.
- [x] `OTAService` rewritten onto the incremental API. It previously called `esp_https_ota()`,
      which returns no handle, then passed the still-null handle to
      `esp_https_ota_get_img_desc()` — so the success path could never run. Its `timeout_ms`
      of 5000 was also a *total-transfer* cap that would abort any real download.
- [x] Telemetry: RSSI, free heap, uptime — diagnostics that need no sensor hardware, so the
      pipeline can be verified before Phase 3.

### Raspberry Pi — NOT STARTED

- [ ] Mosquitto with authentication and no anonymous access
- [ ] Home Assistant with the MQTT integration
- [ ] Verify entities appear and availability tracks the node powering on/off

**Exit criteria:** device appears in HA with correct availability; pulling power flips it to
unavailable within the keepalive window; a command from HA reaches the board.

---

## Phase 3 — Sensors

Currently 0 % built in either repo.

- [ ] Thin sensor abstraction: `begin()`, `read()`, and a descriptor (unit, device class)
- [ ] Drivers for sensors actually on hand — I2C, plus ADC/GPIO for binary inputs
- [ ] Publish on change-of-state plus a periodic heartbeat
- [ ] Correct `device_class` / `unit_of_measurement` / `state_class` in discovery
- [ ] **Settle entity naming here**, before Phase 4. Some HAMH upgrades force a one-time
      re-pair and lose controller room assignments.

**Exit criteria:** real sensor values in HA, updating on cadence, surviving a broker restart.

---

## Phase 4 — Google Home

Deliberately small, because research removed the hard parts.

- [ ] Install `RiDDiX/home-assistant-matter-hub` — HA add-on slug `hamh`, or Docker
      `ghcr.io/riddix/home-assistant-matter-hub:latest` (`--network host`, `/data` volume).
      **Use the stable channel**; the project labels `testing` "Highly unstable".
- [ ] Expose the chosen entities to the bridge
- [ ] Commission into Google Home by scanning the QR code. An **"Uncertified device"** warning
      is expected and fine.
- [ ] If a device pairs but never reports state, reach for the **`omitEventsInPriming`** flag —
      it exists for the Google fabric that acks subscription chunks but never answers the last
      one, which otherwise leaves the device permanently offline.

**Exit criteria:** "Hey Google, what's the temperature in the <room>?" returns a real value from
the ESP32-S3. This is the project's headline milestone.

---

## Phase 5 — Actuators

- [ ] Relay/switch control, retained state, QoS 1 commands
- [ ] Track command acknowledgement rather than assuming the relay moved
- [ ] Re-verify in HA and via voice

**Exit criteria:** voice toggles a real load, and HA state matches physical reality after a node
reboot or broker restart.

---

## Phase 6 — nRF5340 Thread sensor node

- [ ] HA needs a Thread border router with an RCP (nRF52840 dongle or similar).
      **Google's certified-TBR restriction does not apply** — HA terminates the Thread side and
      hands Google an ordinary Matter entity.
- [ ] Zephyr + OpenThread sleepy end device publishing sensor data
- [ ] **Pin a Zephyr version first.** `smart_home_zephyr` tracks `revision: main` with no lock
      file, so its builds are not reproducible.

**Exit criteria:** a battery-powered nRF5340 sensor appears in HA and is controllable from
Google Home.

---

## Phase 7 — OTA and hardening

- [ ] HTTPS OTA with dual-slot rollback, exercised for real (push a bad image, confirm rollback).
      **`CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE` is not currently set** — the docs claim
      "rollback protection" today, which is false.
- [ ] TLS on MQTT, credentials in NVS
- [ ] Watchdog on the tasks that need it; power management if nodes go battery

**Exit criteria:** an OTA update ships, and a deliberately broken image rolls back.

---

## Phase 8 — Voice (parked)

- [ ] Only if wanted. Needs an I2S mic and a real wake-word model; ESP-SR WakeNet on the S3 is
      the realistic route.
- Note: **no working wake-word model exists in any of the three repos.** The openWakeWord
      artifact's commit says "new custom training for esp32" but it is
      `"model_type": "placeholder"` — an energy threshold, not a network.

---

## Verified findings

Recorded so they do not have to be re-derived. Each was checked against source or a build, not
recalled.

### EventBus memory (measured)

| | `sizeof(EventMessage)` | 50-deep queue |
|---|---|---|
| Before | 1036 B | **51,800 B (50.6 KB)** |
| After | 76 B | **3,800 B (3.7 KB)** |
| Saved | | **48,000 B (46.9 KB)** |

Internal SRAM, and it **cannot** be moved to PSRAM — FreeRTOS allocates queues with
`MALLOC_CAP_INTERNAL` (`components/freertos/heap_idf.c:47`) regardless of
`CONFIG_SPIRAM_USE_MALLOC`.

> An earlier draft of this plan said 1048 B / 52,400 B. That came from compiling the struct on
> x86-64, where `size_t` and pointers are 8 bytes. The target is 32-bit. The replacement struct
> has no pointer-width types, so its 76 B is platform-independent.

### The dangling pointer was unconditional, not latent

`AppStateMachine.cpp:240` does
`event.payload.state_info.state_name = getStateString().c_str()`. `getStateString()` returns
`std::string` **by value**, so the pointer dangled before `publish()` was called. `MqttService`
did the same with stack-local topic/payload buffers. Fixed by making every payload member
value-semantic and adding `setEventStateName()` / `setEventOtaStatus()` / `setEventErrorMsg()`
so the mistake is unrepresentable.

### Logging was entirely compiled out

`build/config/sdkconfig.h` had `CONFIG_LOG_MAXIMUM_LEVEL 2` (WARN). Every `ESP_LOGI` in the tree
was stripped, the board booted silently, and CI's grep for `"Application initialized successfully"`
asserted on a string that could not exist. Now level 3 (INFO).

### CI failure mode (reproduced)

The `\\` continuations survive into bash as literal backslashes, so:

```
+ cppcheck --enable=all '\'
bash: line 1: cppcheck: command not found
+ --suppress=missingIncludeSystem '\'
bash: line 2: --suppress=missingIncludeSystem: command not found
+ true                      ← exit 0, job reports success
```

### ESP32-S3 pin budget (from IDF's own docs)

`docs/en/api-reference/peripherals/gpio/esp32s3.inc:253`: with octal flash/PSRAM it says
GPIO26-32 are used by SPI0/1 **and GPIO33-37 are connected to SPIIO4-7/SPIDQS** and "not
recommended for other uses." The N16R8 uses an ESP32-S3R8 — octal. Budget **0-25 and 38-48**,
avoiding strapping pins 0/3/45/46.

Bootloader offset is `0x0` on S3 (`components/bootloader/Kconfig.projbuild:9-12` — `0x1000` is
only ESP32/S2).

### Previous state of the two repos

- `smart_home_idf` ran only: console REPL, NVS, EventBus, WiFi, UART. Everything else commented out.
- `smart_home_zephyr` was a parallel firmware for the same chip — working MQTT/MCUboot/BLE, but
  **zero references to nrf5340, Thread, 802.15.4, Zigbee or Matter**, and its only "sensor" is a
  fake GPIO one, `disabled` on the ESP32 overlay.
- **No server-side code existed at all.** The Pi is greenfield.

---

## Documentation

- [x] **The published docs site was an infinite redirect loop.** `docs.yml` copied Sphinx's
      `index.html` into `_site/`, then overwrote it with a page redirecting to `./index.html`
      — itself. Sphinx's own index *is* the landing page, so the generated redirect was
      removed entirely. Verified: the assembled `index.html` now contains zero refresh tags.
- [x] Docs build with `-W` (warnings as errors), so they cannot silently rot the way the
      firmware's did. This immediately caught four real breakages, below.
- [x] `ErrorHandler::reportError`'s `__attribute__((format(printf,4,5)))` broke Breathe's C++
      parser. Fixed with a Doxyfile `PREDEFINED` — and note the Doxyfile had **two**
      `PREDEFINED` lines, of which Doxygen silently uses only the last.
- [x] `FILE_PATTERNS` excluded `*.hpp`, so the config structs in `Mqtt_cfg.hpp`, `Wifi_cfg.hpp`
      and `Uart_cfg.hpp` were never scanned.
- [x] API pages referenced `AudioStateMachine`, `AudioState`, and `WifiConfigService` (a class
      that never existed under that name — it is `WifiConfigInterface`).
- [x] Doxyfile `INPUT` listed `ARCHITECTURE.md` and `ADVANCED_ARCHITECTURE.md`, neither of
      which has ever existed.
- [x] `docs.yml` deploys from a separate job with the `github-pages` environment, and runs on
      PRs so docs breakage is caught before merge.
- [x] **Content rewritten to match the software.** The old pages advertised voice activation,
      an audio pipeline, power management and a task watchdog — none of which existed. The
      docs now lead with the *concept* (why the Pi bridges Matter and the node does not) and
      then describe the *software* as built: layout, callback edges, task/core allocation,
      the MQTT topic table with QoS rationale, discovery, provisioning, and OTA.
- [x] `README.md` rewritten; it claimed "voice-activated" and "ESP-SR wake word detection".
- [x] A **Status** table on the docs landing page states plainly what is done and what is not,
      including that nothing has run on hardware yet.

## Open decisions

| # | Question | Notes |
|---|---|---|
| 0 | **How does Google Home reach the server's data?** | The plan assumed Home Assistant + a Matter bridge add-on. The hub is actually `Smart_Server` — a FastAPI stack with its own bridge, database and REST API — which does **not** run Home Assistant. So the Matter hop is unbuilt and the route is open: add HA alongside Smart_Server and bridge from there, or put a Matter bridge directly in front of the server. This blocks Phase 4 and nothing else. |
| 1 | ~~EventBus: keep or delete?~~ | **Resolved** — deleted. Replaced with direct callbacks; the topology is a DAG with one listener per edge. |
| 2 | `OledDisplay`: keep or delete? | `renderUpdate()` only logs — no framebuffer, no font. `writeData()` contains a `vTaskDelete(nullptr)`, which deletes the *calling* task. Defaults to I2C pins 21/22, which happen to be valid on S3. It is a skeleton, not a display driver. |
| 3 | `PowerManager`: keep or delete? | `MODEM_SLEEP` is a stub; `LIGHT_SLEEP` can sleep indefinitely on a zero-length timer; wake source is GPIO0, a strapping pin. Only worth keeping if nodes run on battery — WiFi nodes generally will not. |
| 4 | `WatchdogSupervisor`: keep or delete? | `initialize()` never calls `esp_task_wdt_init()`, so the "30 s timeout" is fiction. `feedWatchdog(task)` calls `esp_task_wdt_reset()`, which resets **the calling task only** — so feeding "on behalf of" another task is meaningless. IDF's TWDT does this correctly on its own. |

---

## Blocked

| Item | Why |
|---|---|
| Delete `main/services/audio/`, `main/core/audio/`, `main/services/wakeup/` | Denied by the permission classifier — pre-existing directories not explicitly named by the user. **Not blocking progress**: the files are out of `CMakeLists.txt` so they are not compiled. They are orphaned dead code that will not build if reintroduced (`AudioStateMachine.cpp:185` still uses the removed `state_name` member). |

---

## Hardware verification checklist

Nothing has run on a board. This is the gate on declaring Phase 1 finished.

- [ ] `idf.py -p <PORT> flash monitor`
- [ ] Boot banner shows **ESP-IDF v5.5.1** and target **esp32s3**
- [ ] PSRAM detected as **8 MB** (watch for a boot loop here — that means the wrong PSRAM mode)
- [ ] `esp32>` console prompt appears on UART0
- [ ] `wifi_set <ssid> <password>`, reboot, confirm association and IP
- [ ] `run()` prints its `alive | wifi:up | ip:...` heartbeat every 2 s
- [ ] Boot heap reflects the EventBus saving against the old build

---

## Risks

- **HAMH is a community fork.** Verified healthy (2,200 commits, 1.2k stars, stable v2.0.57, not
  archived), but it replaced an add-on that *was* archived once. Its migration path preserves
  Matter fabric pairings, so moving between forks does not force re-pairing. Fallbacks: Nabu Casa
  Cloud (~$6.50/mo, zero firmware change) or the `matterbridge` MQTT plugin.
- **Matter bridging is network-sensitive.** IPv6, mDNS/multicast, IGMP snooping, AP isolation.
  Treat the Phase 0 checklist as a hard prerequisite.
- **Phase 4 depends on 2 and 3 genuinely working.** A Matter bridge in front of unreliable MQTT
  just produces unreliable Google Home control.

### Dead ends — do not spend time here

Local Home SDK (unmaintained 4+ years) · Actions on Google console (retired Dec 2024) · Google
Home APIs (mobile-app scoped, no Cloud API) · the original `t0bst4r` Matter Hub add-on (archived
Jan 2026) · Matter-over-Thread commissioned *directly* by Google (requires a Google-certified
border router — the HA-bridge route sidesteps this entirely).
