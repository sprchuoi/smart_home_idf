#pragma once

// Intentionally empty, and included by nothing.
//
// This header previously defined a namespace-scope
// `const esp_task_wdt_config_t wdt_config`, whose only consumer was a
// commented-out call in Application::initialize(). Three things were wrong
// with it:
//
//   1. `idle_core_mask = CONFIG_ESP_MAIN_TASK_AFFINITY` is a type confusion.
//      That config is a core *affinity selector* (0x0 CPU0 / 0x1 CPU1 /
//      0x7FFFFFFF for none), whereas idle_core_mask is a core *bitmask* where
//      `1 << i` monitors core i's idle task. With the default CPU0 affinity it
//      evaluated to 0x0 -- i.e. no idle task monitored at all. With CPU1 it
//      would have monitored the wrong core; with no-affinity it would have set
//      31 bits on a 2-core chip.
//   2. It included "app/src/Application.h" from a config header, which is
//      inside-out -- callers wanting Application.h should include it directly.
//   3. The 30 s timeout it advertised was fiction. WatchdogSupervisor::
//      initialize() never called esp_task_wdt_init(), so the real TWDT stayed
//      at the 5 s default from sdkconfig.
//
// The timeout now lives where it actually takes effect:
//     CONFIG_ESP_TASK_WDT_TIMEOUT_S=30   in sdkconfig.defaults
//
// Kept as a tombstone rather than removed, so the reasoning is discoverable at
// the path anyone would look at first.
