/**
 * @file main.cpp
 * @brief ESP32 Smart Home Application Entry Point
 * 
 * Multi-core ESP32 application using ESP-IDF v5.x
 * 
 * CORE ALLOCATION:
 * - Core 0: Networking (WiFi, MQTT)
 * - Core 1: Application logic (State Machine, Display, Wake Word)
 */

#include <stdio.h>
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "app/Application.h"

static const char* TAG = "main";

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "ESP32 Smart Home Application");
    ESP_LOGI(TAG, "ESP-IDF Version: %s", esp_get_idf_version());
    ESP_LOGI(TAG, "FreeRTOS Version: %s", tskKERNEL_VERSION_NUMBER);
    ESP_LOGI(TAG, "========================================");
    
    // Create and initialize application
    Application app;
    
    if (!app.initialize()) {
        ESP_LOGE(TAG, "Failed to initialize application");
        return;
    }
    
    if (!app.start()) {
        ESP_LOGE(TAG, "Failed to start application");
        return;
    }
    
    // Run application (blocks)
    app.run();
    
    ESP_LOGI(TAG, "Application exited");
}

