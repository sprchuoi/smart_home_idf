/**
 * @file OTAService.h
 * @brief OTA Update Service (Core 0)
 * 
 * HTTPS OTA updates with rollback protection.
 * Blocks sleep modes during OTA.
 */

#pragma once

#include "core/eventbus/EventBus.h"
#include "esp_https_ota.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string>

/**
 * @brief OTA Service
 * 
 * Handles HTTPS OTA updates with progress reporting
 */
class OTAService {
public:
    OTAService();
    ~OTAService();
    
    /**
     * @brief Initialize OTA service
     * @return true on success
     */
    bool initialize();
    
    /**
     * @brief Start OTA update
     * @param url HTTPS URL of firmware image
     * @return true on success
     */
    bool startOTA(const char* url);
    
    /**
     * @brief Check if OTA is in progress
     * @return true if OTA is active
     */
    bool isOTAInProgress() const { return m_ota_in_progress; }
    
    /**
     * @brief Stop OTA service
     */
    void stop();
    
    /**
     * @brief FreeRTOS task entry point
     */
    static void taskEntry(void* parameter);

private:
    void taskLoop();
    void publishProgress(uint32_t percent, const char* status);
    
    TaskHandle_t m_task_handle;
    SemaphoreHandle_t m_ota_mutex;
    bool m_initialized;
    bool m_ota_in_progress;
    std::string m_ota_url;
    
    static const char* TAG;
    static constexpr int TASK_STACK_SIZE = 8192;
    static constexpr int TASK_PRIORITY = 6;
    static constexpr BaseType_t TASK_CORE = 0;  // Core 0
};

