/**
 * @file PowerManager.cpp
 * @brief Power Manager implementation
 */

#include "PowerManager.h"
#include "error/ErrorHandler.h"

const char* PowerManager::TAG = "PowerManager";

PowerManager::PowerManager()
    : m_task_handle(nullptr)
    , m_current_mode(PowerMode::NORMAL)
    , m_last_wake_source(WakeSource::UNKNOWN)
    , m_initialized(false)
    , m_sleep_blocked(false)
    , m_sleep_event_group(nullptr) {
}

PowerManager::~PowerManager() {
    stop();
}

bool PowerManager::initialize() {
    if (m_initialized) {
        return true;
    }
    
    // Create event group for sleep coordination
    m_sleep_event_group = xEventGroupCreate();
    if (m_sleep_event_group == nullptr) {
        ESP_LOGE(TAG, "Failed to create event group");
        return false;
    }
    
    // Configure wake-up sources
    configureWakeSources();
    
    // Subscribe to relevant events
    EventBus::getInstance().subscribe(EventType::WIFI_CONNECTED, [this](const EventMessage& event) {
        (void)event;
        xEventGroupSetBits(m_sleep_event_group, BIT_WIFI_ACTIVE);
    });
    
    EventBus::getInstance().subscribe(EventType::WIFI_DISCONNECTED, [this](const EventMessage& event) {
        (void)event;
        xEventGroupClearBits(m_sleep_event_group, BIT_WIFI_ACTIVE);
    });
    
    EventBus::getInstance().subscribe(EventType::MQTT_CONNECTED, [this](const EventMessage& event) {
        (void)event;
        xEventGroupSetBits(m_sleep_event_group, BIT_MQTT_ACTIVE);
    });
    
    EventBus::getInstance().subscribe(EventType::MQTT_DISCONNECTED, [this](const EventMessage& event) {
        (void)event;
        xEventGroupClearBits(m_sleep_event_group, BIT_MQTT_ACTIVE);
    });
    
    // Create task on Core 1
    BaseType_t result = xTaskCreatePinnedToCore(
        taskEntry,
        "PowerManager",
        TASK_STACK_SIZE,
        this,
        TASK_PRIORITY,
        &m_task_handle,
        TASK_CORE
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create power manager task");
        vEventGroupDelete(m_sleep_event_group);
        m_sleep_event_group = nullptr;
        return false;
    }
    
    m_initialized = true;
    ESP_LOGI(TAG, "PowerManager initialized (Core %d)", TASK_CORE);
    return true;
}

bool PowerManager::requestPowerMode(PowerMode mode) {
    if (mode == m_current_mode) {
        return true;
    }
    
    ESP_LOGI(TAG, "Power mode change requested: %d -> %d",
            static_cast<int>(m_current_mode),
            static_cast<int>(mode));
    
    switch (mode) {
        case PowerMode::NORMAL:
            resumeServices();
            m_current_mode = mode;
            break;
            
        case PowerMode::MODEM_SLEEP:
            // WiFi modem sleep (handled by WiFi service)
            m_current_mode = mode;
            break;
            
        case PowerMode::LIGHT_SLEEP:
            if (canEnterSleep()) {
                suspendServices();
                m_current_mode = mode;
                enterLightSleep(0);  // Indefinite sleep
            } else {
                ESP_LOGW(TAG, "Cannot enter sleep - system active");
                return false;
            }
            break;
    }
    
    return true;
}

bool PowerManager::canEnterSleep() const {
    if (m_sleep_blocked) {
        return false;
    }
    
    // Check if any critical services are active
    EventBits_t bits = xEventGroupGetBits(m_sleep_event_group);
    
    // Can sleep if no WiFi, MQTT, Audio, or OTA activity
    return (bits & (BIT_WIFI_ACTIVE | BIT_MQTT_ACTIVE | BIT_AUDIO_ACTIVE | BIT_OTA_ACTIVE)) == 0;
}

WakeSource PowerManager::enterLightSleep(uint32_t duration_ms) {
    ESP_LOGI(TAG, "Entering light sleep (duration: %lu ms)", duration_ms);
    
    // Configure wake-up sources
    esp_sleep_enable_timer_wakeup(duration_ms * 1000);  // Convert to microseconds
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0);  // GPIO wake-up (example)
    
    // Enter light sleep
    esp_light_sleep_start();
    
    // Determine wake-up source
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    
    WakeSource source = WakeSource::UNKNOWN;
    switch (cause) {
        case ESP_SLEEP_WAKEUP_EXT0:
            source = WakeSource::UART;  // GPIO wake-up
            break;
        case ESP_SLEEP_WAKEUP_TIMER:
            source = WakeSource::TIMER;
            break;
        case ESP_SLEEP_WAKEUP_UNDEFINED:
        default:
            source = WakeSource::UNKNOWN;
            break;
    }
    
    m_last_wake_source = source;
    ESP_LOGI(TAG, "Woke up from light sleep (source: %d)", static_cast<int>(source));
    
    resumeServices();
    m_current_mode = PowerMode::NORMAL;
    
    return source;
}

void PowerManager::stop() {
    if (m_task_handle != nullptr) {
        vTaskDelete(m_task_handle);
        m_task_handle = nullptr;
    }
    
    if (m_sleep_event_group != nullptr) {
        vEventGroupDelete(m_sleep_event_group);
        m_sleep_event_group = nullptr;
    }
    
    m_initialized = false;
}

void PowerManager::taskLoop() {
    ESP_LOGI(TAG, "Power manager task started on Core %d", xPortGetCoreID());
    
    while (true) {
        // Check if system can enter sleep
        if (m_current_mode == PowerMode::NORMAL && canEnterSleep()) {
            // Wait a bit before entering sleep
            vTaskDelay(pdMS_TO_TICKS(5000));
            
            if (canEnterSleep()) {
                requestPowerMode(PowerMode::LIGHT_SLEEP);
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));  // Check every second
    }
}

void PowerManager::handleEvent(const EventMessage& event) {
    (void)event;  // Event handling done via event group bits
}

void PowerManager::configureWakeSources() {
    // Configure GPIO wake-up (example: GPIO 0)
    // In real implementation, configure UART RX wake-up, etc.
    ESP_LOGI(TAG, "Wake-up sources configured");
}

void PowerManager::suspendServices() {
    ESP_LOGI(TAG, "Suspending services for sleep");
    
    // Publish sleep event
    EventMessage event;
    event.type = EventType::STATE_CHANGED;
    event.source = EventSource::APPLICATION;
    event.destination = EventSource::APPLICATION;
    event.payload.state_info.state_name = "SLEEP";
    
    EventBus::getInstance().publish(event);
}

void PowerManager::resumeServices() {
    ESP_LOGI(TAG, "Resuming services after wake");
    
    // Publish wake event
    EventMessage event;
    event.type = EventType::STATE_CHANGED;
    event.source = EventSource::APPLICATION;
    event.destination = EventSource::APPLICATION;
    event.payload.state_info.state_name = "RUNNING";
    
    EventBus::getInstance().publish(event);
}

void PowerManager::taskEntry(void* parameter) {
    PowerManager* pm = static_cast<PowerManager*>(parameter);
    pm->taskLoop();
}

