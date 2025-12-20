/**
 * @file AppStateMachine.cpp
 * @brief Application State Machine implementation
 */

#include "AppStateMachine.h"
#include "error/ErrorHandler.h"
#include "core/watchdog/WatchdogSupervisor.h"

const char* AppStateMachine::TAG = "AppStateMachine";

AppStateMachine::AppStateMachine()
    : m_task_handle(nullptr)
    , m_current_state(AppState::INIT)
    , m_initialized(false) {
}

AppStateMachine::~AppStateMachine() {
    stop();
}

bool AppStateMachine::initialize() {
    if (m_initialized) {
        return true;
    }
    
    // Subscribe to relevant events
    EventBus::getInstance().subscribe(EventType::WIFI_STARTED, [this](const EventMessage& event) {
        handleEvent(event);
    });
    
    EventBus::getInstance().subscribe(EventType::WIFI_CONNECTED, [this](const EventMessage& event) {
        handleEvent(event);
    });
    
    EventBus::getInstance().subscribe(EventType::WIFI_DISCONNECTED, [this](const EventMessage& event) {
        handleEvent(event);
    });
    
    EventBus::getInstance().subscribe(EventType::WIFI_GOT_IP, [this](const EventMessage& event) {
        handleEvent(event);
    });
    
    EventBus::getInstance().subscribe(EventType::MQTT_CONNECTED, [this](const EventMessage& event) {
        handleEvent(event);
    });
    
    EventBus::getInstance().subscribe(EventType::MQTT_DISCONNECTED, [this](const EventMessage& event) {
        handleEvent(event);
    });
    
    EventBus::getInstance().subscribe(EventType::SYSTEM_ERROR, [this](const EventMessage& event) {
        handleEvent(event);
    });
    
    EventBus::getInstance().subscribe(EventType::OTA_STARTED, [this](const EventMessage& event) {
        handleEvent(event);
    });
    
    EventBus::getInstance().subscribe(EventType::WAKE_UP, [this](const EventMessage& event) {
        handleEvent(event);
    });
    
    // Create task on Core 1
    BaseType_t result = xTaskCreatePinnedToCore(
        taskEntry,
        "AppStateMachine",
        TASK_STACK_SIZE,
        this,
        TASK_PRIORITY,
        &m_task_handle,
        TASK_CORE
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create state machine task");
        return false;
    }
    
    m_initialized = true;
    transitionTo(AppState::INIT);
    ESP_LOGI(TAG, "AppStateMachine initialized (Core %d)", TASK_CORE);
    return true;
}

void AppStateMachine::stop() {
    if (m_task_handle != nullptr) {
        vTaskDelete(m_task_handle);
        m_task_handle = nullptr;
    }
    
    m_initialized = false;
}

void AppStateMachine::taskEntry(void* parameter) {
    AppStateMachine* instance = static_cast<AppStateMachine*>(parameter);
    instance->taskLoop();
}

std::string AppStateMachine::getStateString() const {
    switch (m_current_state) {
        case AppState::INIT: return "INIT";
        case AppState::WIFI_CONNECTING: return "WIFI_CONNECTING";
        case AppState::WIFI_CONNECTED: return "WIFI_CONNECTED";
        case AppState::MQTT_CONNECTING: return "MQTT_CONNECTING";
        case AppState::RUNNING: return "RUNNING";
        case AppState::OTA_UPDATING: return "OTA_UPDATING";
        case AppState::ERROR: return "ERROR";
        case AppState::SLEEP: return "SLEEP";
        default: return "UNKNOWN";
    }
}

void AppStateMachine::taskLoop() {
    ESP_LOGI(TAG, "State machine task started on Core %d", xPortGetCoreID());
    
    // Process events from EventBus
    while (true) {
        EventBus::getInstance().processEvents();
        // Feed watchdog for state machine and yield briefly
        if (WatchdogSupervisor::getInstance()) {
            WatchdogSupervisor::getInstance()->feedWatchdog(WatchdogTask::APP_STATE_MACHINE);
        }
        vTaskDelay(pdMS_TO_TICKS(10));  // Small delay to prevent tight loop
    }
}

void AppStateMachine::handleEvent(const EventMessage& event) {
    ESP_LOGI(TAG, "Handling event type %d in state %s",
            static_cast<int>(event.type),
            getStateString().c_str());
    
    switch (m_current_state) {
        case AppState::INIT:
            if (event.type == EventType::WIFI_STARTED) {
                transitionTo(AppState::WIFI_CONNECTING);
            }
            break;
            
        case AppState::WIFI_CONNECTING:
            if (event.type == EventType::WIFI_CONNECTED) {
                // Stay in WIFI_CONNECTING until we get IP
            } else if (event.type == EventType::WIFI_GOT_IP) {
                transitionTo(AppState::WIFI_CONNECTED);
            } else if (event.type == EventType::WIFI_DISCONNECTED || 
                      event.type == EventType::WIFI_ERROR ||
                      event.type == EventType::SYSTEM_ERROR) {
                transitionTo(AppState::ERROR);
            }
            break;
            
        case AppState::WIFI_CONNECTED:
            if (event.type == EventType::MQTT_CONNECTED) {
                transitionTo(AppState::RUNNING);
            } else if (event.type == EventType::WIFI_DISCONNECTED ||
                      event.type == EventType::WIFI_ERROR ||
                      event.type == EventType::SYSTEM_ERROR) {
                transitionTo(AppState::ERROR);
            }
            break;
            
        case AppState::MQTT_CONNECTING:
            if (event.type == EventType::MQTT_CONNECTED) {
                transitionTo(AppState::RUNNING);
            } else if (event.type == EventType::MQTT_DISCONNECTED ||
                      event.type == EventType::MQTT_ERROR ||
                      event.type == EventType::SYSTEM_ERROR) {
                transitionTo(AppState::ERROR);
            }
            break;
            
        case AppState::RUNNING:
            if (event.type == EventType::WIFI_DISCONNECTED) {
                transitionTo(AppState::WIFI_CONNECTING);
            } else if (event.type == EventType::MQTT_DISCONNECTED) {
                transitionTo(AppState::MQTT_CONNECTING);
            } else if (event.type == EventType::OTA_STARTED) {
                transitionTo(AppState::OTA_UPDATING);
            } else if (event.type == EventType::SYSTEM_ERROR) {
                transitionTo(AppState::ERROR);
            } else if (event.type == EventType::POWER_MODE_CHANGED) {
                // Check if entering sleep
                if (event.payload.power_info.mode == 2) {  // LIGHT_SLEEP
                    transitionTo(AppState::SLEEP);
                }
            }
            break;
            
        case AppState::OTA_UPDATING:
            // Stay in OTA state until restart
            break;
            
        case AppState::SLEEP:
            if (event.type == EventType::WAKE_UP || event.type == EventType::WAKE_WORD_DETECTED) {
                transitionTo(AppState::RUNNING);
            }
            break;
            
        case AppState::ERROR:
            // Can transition out of error state on recovery
            if (event.type == EventType::WIFI_CONNECTED) {
                transitionTo(AppState::WIFI_CONNECTED);
            } else if (event.type == EventType::WAKE_UP) {
                transitionTo(AppState::RUNNING);
            }
            break;
    }
}

void AppStateMachine::transitionTo(AppState new_state) {
    if (new_state == m_current_state) {
        return;
    }
    
    ESP_LOGI(TAG, "State transition: %s -> %s",
            getStateString().c_str(),
            [new_state]() {
                switch (new_state) {
                    case AppState::INIT: return "INIT";
                    case AppState::WIFI_CONNECTING: return "WIFI_CONNECTING";
                    case AppState::WIFI_CONNECTED: return "WIFI_CONNECTED";
                    case AppState::MQTT_CONNECTING: return "MQTT_CONNECTING";
                    case AppState::RUNNING: return "RUNNING";
                    case AppState::OTA_UPDATING: return "OTA_UPDATING";
                    case AppState::ERROR: return "ERROR";
                    case AppState::SLEEP: return "SLEEP";
                    default: return "UNKNOWN";
                }
            }());
    
    onStateExit(m_current_state);
    m_current_state = new_state;
    onStateEnter(m_current_state);
    
    // Publish state change event
    EventMessage event;
    event.type = EventType::STATE_CHANGED;
    event.source = EventSource::STATE_MACHINE;
    event.destination = EventSource::APPLICATION;
    event.payload.state_info.state_name = getStateString().c_str();
    
    EventBus::getInstance().publish(event);
}

void AppStateMachine::onStateEnter(AppState state) {
    switch (state) {
        case AppState::INIT:
            ESP_LOGI(TAG, "Entering INIT state");
            break;
            
        case AppState::WIFI_CONNECTING:
            ESP_LOGI(TAG, "Entering WIFI_CONNECTING state");
            break;
            
        case AppState::WIFI_CONNECTED:
            ESP_LOGI(TAG, "Entering WIFI_CONNECTED state");
            // Trigger MQTT connection attempt
            {
                EventMessage event;
                event.type = EventType::MQTT_CONNECTING;
                event.source = EventSource::STATE_MACHINE;
                event.destination = EventSource::MQTT_SERVICE;
                EventBus::getInstance().publish(event);
            }
            transitionTo(AppState::MQTT_CONNECTING);
            break;
            
        case AppState::MQTT_CONNECTING:
            ESP_LOGI(TAG, "Entering MQTT_CONNECTING state");
            break;
            
        case AppState::RUNNING:
            ESP_LOGI(TAG, "Entering RUNNING state - System ready");
            break;
            
        case AppState::OTA_UPDATING:
            ESP_LOGI(TAG, "Entering OTA_UPDATING state");
            break;
            
        case AppState::ERROR:
            ESP_LOGE(TAG, "Entering ERROR state");
            break;
            
        case AppState::SLEEP:
            ESP_LOGI(TAG, "Entering SLEEP state");
            break;
    }
}

void AppStateMachine::onStateExit(AppState state) {
    // Handle state exit logic if needed
    (void)state;  // Suppress unused parameter warning
}

