# Standard — Engineering conventions

Portable rules for any change to this firmware. Each is a single present-state
rule.

- **Reuse before adding** — search for an existing helper, module or console
  helper before writing new code. The console registry
  (`main/core/console/Console.h`) and the two config interfaces are the existing
  patterns to follow.
- **Smallest change that satisfies the rule** — no speculative scope, no drive-by
  refactors bundled with a fix.
- **One module per component** — never fold an interface into its only consumer.
  A service lives under `main/services/<name>/`, its configuration struct under
  `main/services/<name>/cfg/`, and its NVS store beside it.
- **Dependencies point one way** — lower layers never import higher ones. Where
  the need seems to invert, invert it with a `std::function` callback registered
  at the composition root (`Application`), which is how the WiFi and MQTT edges
  work. Do **not** reintroduce a shared event bus.
- **Extract pure cores** — separate the decision from the I/O. Topic
  construction, JSON body construction, sensor payload formatting and
  device-id validation are the seams the host tier reaches; keep them free of
  ESP-IDF dependencies so they are host-testable.
- **Verification before commit** — `./make.sh build`, `./make.sh smoke` and
  `./make.sh test` all pass. A change that cannot be exercised on hardware is
  still built and smoke-checked.
- **Errors fail loudly** — actionable messages, no silent catches, no swallowed
  return codes. Report through `ErrorHandler::reportError` with the right
  category so the failure is attributable.
- **Secrets are never in code or documentation** — only their *location* is
  documented. WiFi credentials and broker passwords are provisioned at runtime
  into NVS over the console; `main/Kconfig` deliberately defines no credential
  symbol.
- **No globals** — singletons are reserved for the two NVS config interfaces,
  and only because an `esp_console` handler is a bare function pointer with
  nowhere else to reach the store.
- **Pin tasks explicitly** with `xTaskCreatePinnedToCore` and name the core in a
  constant. State the stack size and priority where the task is created.
- **Comment *why*, not what** — the valuable comments explain a non-obvious
  constraint, such as why telemetry is QoS 0 while status is QoS 1.
- **Commits** — `[Type] Imperative summary`, one logical change per commit.
  Never commit generated output (`build/`, `sdkconfig`, `doxygen/`,
  `docs/_build/`), temporary directories or secrets. `sdkconfig.defaults` **is**
  committed; the generated `sdkconfig` is not.
