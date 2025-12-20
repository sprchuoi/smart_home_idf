/**
 * @file OTAService.cpp
 * @brief OTA Service implementation
 */

#include "OTAService.h"
#include "error/ErrorHandler.h"
#include <cstring>

const char* OTAService::TAG = "OTAService";

OTAService::OTAService()
    : m_task_handle(nullptr)
    , m_ota_mutex(nullptr)
    , m_initialized(false)
    , m_ota_in_progress(false) {
}

OTAService::~OTAService() {
    stop();
}

bool OTAService::initialize() {
    if (m_initialized) {
        return true;
    }
    
    m_ota_mutex = xSemaphoreCreateMutex();
    if (m_ota_mutex == nullptr) {
        ESP_LOGE(TAG, "Failed to create OTA mutex");
        return false;
    }
    
    // Create task on Core 0
    BaseType_t result = xTaskCreatePinnedToCore(
        taskEntry,
        "OTAService",
        TASK_STACK_SIZE,
        this,
        TASK_PRIORITY,
        &m_task_handle,
        TASK_CORE
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create OTA task");
        vSemaphoreDelete(m_ota_mutex);
        m_ota_mutex = nullptr;
        return false;
    }
    
    m_initialized = true;
    ESP_LOGI(TAG, "OTAService initialized (Core %d)", TASK_CORE);
    return true;
}

bool OTAService::startOTA(const char* url) {
    if (!m_initialized || url == nullptr) {
        return false;
    }
    
    if (xSemaphoreTake(m_ota_mutex, portMAX_DELAY) != pdTRUE) {
        return false;
    }
    
    if (m_ota_in_progress) {
        xSemaphoreGive(m_ota_mutex);
        ESP_LOGW(TAG, "OTA already in progress");
        return false;
    }
    
    m_ota_url = url;
    m_ota_in_progress = true;
    
    xSemaphoreGive(m_ota_mutex);
    
    // Publish OTA started event
    EventMessage event;
    event.type = EventType::OTA_STARTED;
    event.source = EventSource::OTA_SERVICE;
    event.destination = EventSource::APPLICATION;
    
    EventBus::getInstance().publish(event);
    
    ESP_LOGI(TAG, "OTA update started: %s", url);
    return true;
}

void OTAService::stop() {
    if (m_task_handle != nullptr) {
        vTaskDelete(m_task_handle);
        m_task_handle = nullptr;
    }
    
    if (m_ota_mutex != nullptr) {
        vSemaphoreDelete(m_ota_mutex);
        m_ota_mutex = nullptr;
    }
    
    m_initialized = false;
    m_ota_in_progress = false;
}

void OTAService::taskLoop() {
    ESP_LOGI(TAG, "OTA task started on Core %d", xPortGetCoreID());
    
    while (true) {
        if (m_ota_in_progress) {
            // Perform OTA update
            esp_https_ota_config_t ota_config = {
                .http_config = {
                    .url = m_ota_url.c_str(),
                    .timeout_ms = 5000,
                },
                .partial_http_download = false,
            };
            
            esp_https_ota_handle_t https_ota_handle = nullptr;
            esp_err_t err = esp_https_ota(&ota_config, &https_ota_handle);
            
            if (err == ESP_OK) {
                esp_app_desc_t app_desc;
                err = esp_https_ota_get_img_desc(https_ota_handle, &app_desc);
                if (err == ESP_OK) {
                    ESP_LOGI(TAG, "OTA update successful: %s", app_desc.version);
                    publishProgress(100, "Completed");
                    
                    EventMessage event;
                    event.type = EventType::OTA_COMPLETED;
                    event.source = EventSource::OTA_SERVICE;
                    event.destination = EventSource::APPLICATION;
                    
                    EventBus::getInstance().publish(event);
                    
                    // Restart after OTA
                    vTaskDelay(pdMS_TO_TICKS(2000));
                    esp_restart();
                } else {
                    ESP_LOGE(TAG, "Failed to get app description: %s", esp_err_to_name(err));
                }
            } else {
                ESP_LOGE(TAG, "OTA update failed: %s", esp_err_to_name(err));
                ErrorHandler::getInstance().reportError(
                    ErrorCategory::OTA_ERROR,
                    err,
                    "OTA update failed"
                );
                
                publishProgress(0, "Failed");
                
                EventMessage event;
                event.type = EventType::OTA_FAILED;
                event.source = EventSource::OTA_SERVICE;
                event.destination = EventSource::APPLICATION;
                
                EventBus::getInstance().publish(event);
            }
            
            if (https_ota_handle != nullptr) {
                esp_https_ota_finish(https_ota_handle);
            }
            
            m_ota_in_progress = false;
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void OTAService::publishProgress(uint32_t percent, const char* status) {
    EventMessage event;
    event.type = EventType::OTA_PROGRESS;
    event.source = EventSource::OTA_SERVICE;
    event.destination = EventSource::APPLICATION;
    event.payload.ota_info.progress_percent = percent;
    event.payload.ota_info.status = status;
    
    EventBus::getInstance().publish(event);
}

void OTAService::taskEntry(void* parameter) {
    OTAService* ota = static_cast<OTAService*>(parameter);
    ota->taskLoop();
}

