/**
 * @file main.cpp
 * @brief ESP32-S3 Smart Home application entry point
 *
 * Everything runs on Core 0: the WiFi and lwIP stacks live there, and so do
 * the service tasks. See docs/architecture.rst.
 */

#include <stdio.h>
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "app/src/Application.h"

static const char* TAG = "main";

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "ESP32-S3 Smart Home");
    ESP_LOGI(TAG, "ESP-IDF version: %s", esp_get_idf_version());
    ESP_LOGI(TAG, "FreeRTOS version: %s", tskKERNEL_VERSION_NUMBER);
    ESP_LOGI(TAG, "========================================");

    // Static, not a stack local. Application owns every service plus two
    // config structs, which is a couple of kilobytes -- enough on its own to
    // matter against CONFIG_ESP_MAIN_TASK_STACK_SIZE, and it is what tipped
    // this task into a stack overflow during esp_wifi_init(). It is a
    // singleton in everything but name, so it belongs in .bss.
    static Application app;

    if (!app.initialize()) {
        ESP_LOGE(TAG, "Failed to initialize application");
        return;
    }

    if (!app.start()) {
        ESP_LOGE(TAG, "Failed to start application");
        return;
    }

    // How much of the main task's stack survives initialization. Worth
    // knowing before adding anything else to the init path -- the failure mode
    // otherwise is a crash inside WiFi, which points nowhere near the cause.
    ESP_LOGI(TAG, "Main task stack headroom after init: %u bytes",
             (unsigned)(uxTaskGetStackHighWaterMark(NULL) * sizeof(StackType_t)));

    // Run application (blocks)
    app.run();

    ESP_LOGI(TAG, "Application exited");
}
