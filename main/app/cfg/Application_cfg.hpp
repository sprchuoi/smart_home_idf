#pragma once
#include "app/src/Application.h"

const esp_task_wdt_config_t wdt_config = {
    .timeout_ms = 30000,  // 30 seconds timeout
    .idle_core_mask = CONFIG_ESP_MAIN_TASK_AFFINITY,
    .trigger_panic = true
};