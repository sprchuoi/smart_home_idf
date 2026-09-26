# Project — Tooling and credentials

The toolchain this project pins, the commands that build and verify it, and
where the tools and secrets live. Build and flash *steps* for an operator are in
the OPERATE plane; this file is what the build contract depends on.

## Toolchain pins

| Item | Version / value | Where it is pinned |
|---|---|---|
| ESP-IDF | **v5.5.1** | `make.sh`, `.github/workflows/ci.yml` (`IDF_VERSION`) |
| Target | `esp32s3` | `sdkconfig.defaults`, CI (`IDF_TARGET`) |
| Board | ESP32-S3-DevKitC-1 N16R8 (16 MB flash, 8 MB octal PSRAM) | `sdkconfig.defaults` |
| Partition table | `partitions.csv` (custom) | `sdkconfig.defaults` |
| CMake | ≥ 3.16 (project declares 3.16) | `CMakeLists.txt` |
| Python | 3.8+ | `docs/getting-started.rst` |
| Static analysis | `cppcheck` (installed explicitly in CI) | `.github/workflows/ci.yml` |

`sdkconfig.defaults` is **committed**; the generated `sdkconfig` is
per-machine and ignored.

## Build and verification commands

| Command | Purpose | Can fail? |
|---|---|---|
| `./make.sh setup` | Check the toolchain, install Python dependencies | yes |
| `./make.sh build` | Build the firmware for esp32s3 | yes |
| `./make.sh smoke` | Verify target, flash size, octal PSRAM, log level, OTA partition labels and image fit | **yes — this is the gate before flashing** |
| `./make.sh test` | Static analysis (`cppcheck`) | yes |
| `./make.sh flash-monitor` | Flash and open the serial console | yes |
| `./make.sh monitor` | Serial console only | yes |
| `./make.sh build-ota-test` | Regenerate `sdkconfig` with the plain-HTTP OTA overlay | yes |
| `./make.sh build-production` | Drop the overlay and rebuild | yes |
| `./make.sh doc` | Build Sphinx + Doxygen | yes |
| `./make.sh clean` | Remove build output | yes |
| `./make.sh ci` | Run the pipeline locally, including the smoke gate | yes |

A step that cannot fail does not belong in CI. The QEMU job was removed for that
reason: `qemu-system-xtensa` cannot emulate an ESP32-S3.

## Configuration overlay

| File | Applies | Purpose |
|---|---|---|
| `sdkconfig.defaults` | every build | Production configuration — target, flash, PSRAM, partitions, log level, TWDT, main stack |
| `sdkconfig.ota-test.defaults` | only via `./make.sh build-ota-test` | Enables `CONFIG_ESP_HTTPS_OTA_ALLOW_HTTP`, which lets anyone on the network path substitute the firmware. It lives in its own file so it cannot be enabled by accident. |

## Test and verification tooling

| Path | Owns |
|---|---|
| `testing/test-plan.yaml` | The declared test plan — every case, its tier, equipment and expectation (Phase 1) |
| `testing/gates/` | The project's gate checks, run by CI and by `/build` (Phase 1) |
| `tests/` | Executable tests by tier (`host/`, `target/`, `bench/`) — empty today |
| `tools/mqtt/` | Broker helper, mosquitto config, and the node acceptance script |
| `tools/ota/` | Range-capable static image server and the OTA bench script |

## Credential and secret locations

Never the secret itself — only where it lives.

| Secret | Location | Provisioned by |
|---|---|---|
| WiFi SSID + passphrase | NVS namespace `wifi_config` on the device | Console `wifi_set` / `wifi_ssid` / `wifi_password` |
| MQTT username + password | NVS namespace `mqtt_config` on the device | Console `mqtt_auth` |
| MQTT broker coordinates | NVS namespace `mqtt_config` on the device | Console `mqtt_set` |
| Device identity | NVS namespace `mqtt_config` (`devid`), derived from the MAC when unset | Console `mqtt_device` |
| CI repository token | GitHub Actions secret `GITHUB_TOKEN` | GitHub repository settings |

No secret is committed, and no credential symbol exists in `main/Kconfig`.

## Documentation build

`./make.sh doc` runs Doxygen then Sphinx. The Sphinx build uses `-W` (warnings
as errors), so a broken cross-reference fails the docs job rather than rotting
silently. `.github/workflows/docs.yml` deploys to GitHub Pages.
