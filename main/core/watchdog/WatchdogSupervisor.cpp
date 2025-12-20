/**
 * @file WatchdogSupervisor.cpp
 * @brief Watchdog Supervisor implementation
 */

#include "WatchdogSupervisor.h"
#include "error/ErrorHandler.h"
#include <cstring>

const char* WatchdogSupervisor::TAG = "WatchdogSupervisor";

WatchdogSupervisor::WatchdogSupervisor()
    : m_task_handle(nullptr)
    , m_heartbeat_group(nullptr)
    , m_initialized(false)
    , m_timeout_seconds(30)
    , m_registered_tasks(static_cast<size_t>(WatchdogTask::MAX_TASKS), nullptr)
    , m_last_heartbeat(static_cast<size_t>(WatchdogTask::MAX_TASKS), 0) {
}

WatchdogSupervisor::~WatchdogSupervisor() {
    stop();
}

bool WatchdogSupervisor::initialize(const esp_task_wdt_config_t wdt_config) {
    if (m_initialized) {
        return true;
    }
    
    m_timeout_seconds = wdt_config.timeout_ms / 1000; // set timeout from config (convert ms to seconds)
    
    esp_err_t err = esp_task_wdt_add(NULL); // add current task
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to add task watchdog: %s", esp_err_to_name(err));
        return false;
    }

    // Create event group for heartbeat monitoring
    m_heartbeat_group = xEventGroupCreate();
    if (m_heartbeat_group == nullptr) {
        ESP_LOGE(TAG, "Failed to create heartbeat event group");
        return false;
    }
    
    // Create supervisor task on Core 1
    BaseType_t result = xTaskCreatePinnedToCore(
        taskEntry,
        "WatchdogSupervisor",
        TASK_STACK_SIZE,
        this,
        TASK_PRIORITY,
        &m_task_handle,
        TASK_CORE
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create watchdog supervisor task");
        vEventGroupDelete(m_heartbeat_group);
        m_heartbeat_group = nullptr;
        return false;
    }
    
    m_initialized = true;
    ESP_LOGI(TAG, "WatchdogSupervisor initialized (timeout: %lu s, Core %d)",
            m_timeout_seconds, TASK_CORE);
    return true;
}

bool WatchdogSupervisor::registerTask(WatchdogTask task_id, TaskHandle_t task_handle) {
    if (!m_initialized || task_id >= WatchdogTask::MAX_TASKS) {
        return false;
    }
    
    // Add task to ESP task watchdog
    esp_err_t err = esp_task_wdt_add(task_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add task to watchdog: %s", esp_err_to_name(err));
        return false;
    }
    
    m_registered_tasks[static_cast<size_t>(task_id)] = task_handle;
    m_last_heartbeat[static_cast<size_t>(task_id)] = xTaskGetTickCount();
    
    ESP_LOGI(TAG, "Registered task %d for watchdog monitoring", static_cast<int>(task_id));
    return true;
}

bool WatchdogSupervisor::feedWatchdog(WatchdogTask task_id) {
    if (!m_initialized || task_id >= WatchdogTask::MAX_TASKS) {
        return false;
    }
    
    TaskHandle_t task_handle = m_registered_tasks[static_cast<size_t>(task_id)];
    if (task_handle == nullptr) {
        return false;
    }
    
    // Feed ESP task watchdog
    esp_err_t err = esp_task_wdt_reset();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to feed watchdog: %s", esp_err_to_name(err));
        return false;
    }
    
    // Update heartbeat bit
    int bit = 0;
    switch (task_id) {
        case WatchdogTask::WIFI_SERVICE:
            bit = BIT_WIFI;
            break;
        case WatchdogTask::MQTT_SERVICE:
            bit = BIT_MQTT;
            break;
        case WatchdogTask::APP_STATE_MACHINE:
            bit = BIT_STATE_MACHINE;
            break;
        case WatchdogTask::AUDIO_PIPELINE:
            bit = BIT_AUDIO;
            break;
        case WatchdogTask::WAKE_WORD_SERVICE:
            bit = BIT_WAKE_WORD;
            break;
        default:
            return false;
    }
    
    xEventGroupSetBits(m_heartbeat_group, bit);
    m_last_heartbeat[static_cast<size_t>(task_id)] = xTaskGetTickCount();
    
    return true;
}

bool WatchdogSupervisor::isTaskAlive(WatchdogTask task_id) const {
    if (!m_initialized || task_id >= WatchdogTask::MAX_TASKS) {
        return false;
    }
    
    TickType_t now = xTaskGetTickCount();
    TickType_t last = m_last_heartbeat[static_cast<size_t>(task_id)];
    TickType_t timeout_ticks = pdMS_TO_TICKS(m_timeout_seconds * 1000);
    
    return (now - last) < timeout_ticks;
}

void WatchdogSupervisor::stop() {
    if (m_task_handle != nullptr) {
        vTaskDelete(m_task_handle);
        m_task_handle = nullptr;
    }
    
    if (m_heartbeat_group != nullptr) {
        vEventGroupDelete(m_heartbeat_group);
        m_heartbeat_group = nullptr;
    }
    
    if (m_initialized) {
        esp_task_wdt_deinit();
        m_initialized = false;
    }
}

void WatchdogSupervisor::taskLoop() {
    ESP_LOGI(TAG, "Watchdog supervisor task started on Core %d", xPortGetCoreID());
    
    TickType_t last_check = xTaskGetTickCount();
    const TickType_t check_interval = pdMS_TO_TICKS(5000);  // Check every 5 seconds
    
    while (true) {
        vTaskDelayUntil(&last_check, check_interval);
        
        checkTaskHealth();
    }
}

void WatchdogSupervisor::checkTaskHealth() {
    TickType_t now = xTaskGetTickCount();
    TickType_t timeout_ticks = pdMS_TO_TICKS(m_timeout_seconds * 1000);
    
    for (size_t i = 0; i < static_cast<size_t>(WatchdogTask::MAX_TASKS); i++) {
        if (m_registered_tasks[i] == nullptr) {
            continue;
        }
        
        TickType_t last = m_last_heartbeat[i];
        TickType_t elapsed = now - last;
        
        if (elapsed >= timeout_ticks) {
            const char* task_name = "UNKNOWN";
            switch (static_cast<WatchdogTask>(i)) {
                case WatchdogTask::WIFI_SERVICE:
                    task_name = "WifiService";
                    break;
                case WatchdogTask::MQTT_SERVICE:
                    task_name = "MqttService";
                    break;
                case WatchdogTask::APP_STATE_MACHINE:
                    task_name = "AppStateMachine";
                    break;
                case WatchdogTask::AUDIO_PIPELINE:
                    task_name = "AudioPipeline";
                    break;
                case WatchdogTask::WAKE_WORD_SERVICE:
                    task_name = "WakeWordService";
                    break;
                default:
                    break;
            }
            
            ESP_LOGE(TAG, "Task %s stalled (no heartbeat for %lu ms)",
                    task_name, pdTICKS_TO_MS(elapsed));
            
            ErrorHandler::getInstance().reportError(
                ErrorCategory::SYSTEM_ERROR,
                ESP_ERR_TIMEOUT,
                "Task %s watchdog timeout", task_name
            );
            
            // Trigger safe reset after logging
            triggerSafeReset(task_name);
        }
    }
}

void WatchdogSupervisor::triggerSafeReset(const char* reason) {
    ESP_LOGE(TAG, "Triggering safe reset due to: %s", reason);
    
    // Publish error event
    EventMessage event;
    event.type = EventType::SYSTEM_ERROR;
    event.source = EventSource::ERROR_HANDLER;
    event.destination = EventSource::APPLICATION;
    event.payload.error_info.error_code = ESP_ERR_TIMEOUT;
    strncpy(event.payload.error_info.error_msg, reason, sizeof(event.payload.error_info.error_msg) - 1);
    event.payload.error_info.error_msg[sizeof(event.payload.error_info.error_msg) - 1] = '\0';
    
    EventBus::getInstance().publish(event);
    
    // Give time for event to be processed
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Trigger reset
    esp_restart();
}

void WatchdogSupervisor::taskEntry(void* parameter) {
    WatchdogSupervisor* wd = static_cast<WatchdogSupervisor*>(parameter);
    wd->taskLoop();
}

