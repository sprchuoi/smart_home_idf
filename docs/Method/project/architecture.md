# Project — Architecture bindings

The layers, the source layout that mirrors them, and the rules that keep the
dependency direction honest. The observable architecture is specified in the
FSD, §2.4; this file is the HOW that implements it.

## Layers

| Layer | Modules | Rule |
|---|---|---|
| **L2 Application logic** | `main/app/` (`Application`), `main/core/statemachine/` (`AppStateMachine`) | May depend on L1 and L0. `Application` is the composition root. |
| **L1 Interfaces** | `main/services/wifi/`, `main/services/mqtt/`, `main/services/ota/`, `main/core/console/`, `main/core/system/`, `main/drivers/`, `main/error/` | May depend on L0 only. Never on L2. |
| **L0 Foundation** | ESP-IDF components (`freertos`, `nvs_flash`, `esp_wifi`, `esp_netif`, `esp_event`, `mqtt`, `esp_https_ota`, `esp_http_client`, `console`, `json`, `mbedtls`) | Provided, not implemented. Never modified in-tree. |

## Source layout

```
main/
├── main.cpp                  app_main(): construct, initialize, run
├── app/src/Application.*     composition root — owns services, wires callbacks
├── app/cfg/                  tombstone header; defines nothing
├── core/console/             command registry and grouped help
├── core/statemachine/        application state holder
├── core/system/              device-level console commands
├── services/wifi/            station link + NVS store (+ cfg/)
├── services/mqtt/            MQTT wire + NVS store (+ cfg/)
├── services/ota/             HTTPS OTA receiver
├── drivers/uart/             second-UART driver (+ cfg/); not started
└── error/                    error logging and counters
```

**Every component maps to its own module.** A new service goes under
`main/services/<name>/` with its configuration struct under
`main/services/<name>/cfg/`. Its `.cpp` is added to `SRCS` in
`main/CMakeLists.txt`, and any new component it needs to `REQUIRES`.

## Dependency direction — the rules

1. **L1 never depends on L2.** A service that needs to tell the application
   something exposes a `std::function` callback setter; the application registers
   the callback at the composition root.
2. **L1 modules do not depend on each other.** `WifiService` does not call
   `MqttService`. The application observes the WiFi callback and drives MQTT.
3. **No shared event bus.** The topology is a DAG with one listener per edge.
   A queue that copies payloads re-introduces the dangling-pointer class of bug
   and adds nothing.
4. **L0 is configured, never wrapped gratuitously.** Prefer a specific
   `esp_driver_*` component over the legacy `driver` umbrella.
5. **`Application` is the only object that knows about more than one service.**
   It constructs them, wires them and stops them, in reverse order.
6. **NVS access goes through a config interface**, never through ad-hoc
   `nvs_get_*` calls scattered across services. One namespace per subsystem.

## Prohibitions

- No credential, broker address or device identity compiled into an image.
  `main/Kconfig` defines no configuration symbols; the file is a tombstone.
- No `ESP_ERROR_CHECK` on a per-command path: one failed console command must
  not abort the boot of an otherwise healthy device. Registration failures are
  logged.
- No `vTaskDelete(nullptr)` outside a task's own exit path.
- No task created without an explicit core, stack size and priority.
- No new top-level directory under `main/` without a layer assignment here.

## Adding a service or console command

The step-by-step procedure lives in
[`../../development.rst`](../../development.rst) — bind to it, do not restate it.
The rules it must satisfy are the six dependency rules above and the engineering
standard in [`../standards/engineering.md`](../standards/engineering.md).

## Bound documentation

The pre-existing Sphinx pages `docs/architecture.rst` and
`docs/development.rst` describe the same architecture and are **bound, not
superseded**. Where they disagree with the code, they are stale; the proposed
cleanup is in [`plane-retrofit-plan.md`](plane-retrofit-plan.md).
