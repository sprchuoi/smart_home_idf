/**
 * @file MqttService.cpp
 * @brief MQTT Service implementation
 */

#include "MqttService.h"
#include "error/ErrorHandler.h"
#include "core/watchdog/WatchdogSupervisor.h"
#include <cstring>

const char* MqttService::TAG = "MqttService";

MqttService::MqttService()
    : m_client(nullptr)
    , m_task_handle(nullptr)
    , m_initialized(false)
    , m_connected(false) {
}

MqttService::~MqttService() {
    stop();
}

bool MqttService::initialize(const char* broker_uri, const char* client_id) {
    if (m_initialized) {
        return true;
    }
    
    if (broker_uri == nullptr || client_id == nullptr) {
        ESP_LOGE(TAG, "Invalid parameters");
        return false;
    }
    
    m_broker_uri = broker_uri;
    m_client_id = client_id;
    
    // Configure MQTT client
    esp_mqtt_client_config_t mqtt_cfg = {};
    mqtt_cfg.broker.address.uri = m_broker_uri.c_str();
    mqtt_cfg.credentials.client_id = m_client_id.c_str();
    
    m_client = esp_mqtt_client_init(&mqtt_cfg);
    if (m_client == nullptr) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
        return false;
    }
    
    // Register event handler
    esp_mqtt_client_register_event(m_client,
                                   (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID,
                                   mqttEventHandler,
                                   this);
    
    // Create task on Core 0
    BaseType_t result = xTaskCreatePinnedToCore(
        taskEntry,
        "MqttService",
        TASK_STACK_SIZE,
        this,
        TASK_PRIORITY,
        &m_task_handle,
        TASK_CORE
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create MQTT task");
        esp_mqtt_client_destroy(m_client);
        m_client = nullptr;
        return false;
    }
    
    m_initialized = true;
    ESP_LOGI(TAG, "MqttService initialized (Core %d)", TASK_CORE);
    return true;
}

bool MqttService::connect() {
    if (!m_initialized) {
        ESP_LOGE(TAG, "MqttService not initialized");
        return false;
    }
    
    esp_err_t err = esp_mqtt_client_start(m_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start MQTT client: %s", esp_err_to_name(err));
        ErrorHandler::getInstance().reportError(
            ErrorCategory::MQTT_ERROR,
            err,
            "Failed to start MQTT client"
        );
        return false;
    }
    
    ESP_LOGI(TAG, "MQTT connection initiated");
    return true;
}

void MqttService::disconnect() {
    if (m_initialized && m_client != nullptr) {
        esp_mqtt_client_stop(m_client);
    }
}

bool MqttService::publish(const char* topic, const char* data, size_t data_len) {
    if (!m_connected || m_client == nullptr || topic == nullptr || data == nullptr) {
        return false;
    }
    
    // QoS 0 only (as per requirements)
    int msg_id = esp_mqtt_client_publish(m_client, topic, data, data_len, 0, 0);
    if (msg_id < 0) {
        ESP_LOGE(TAG, "Failed to publish to topic: %s", topic);
        return false;
    }
    
    ESP_LOGI(TAG, "Published to %s (msg_id: %d, QoS 0)", topic, msg_id);

    publishEvent(EventType::MQTT_PUBLISHED);

    return true;
}

bool MqttService::publishHADiscovery(const char* device_name, const char* device_id) {
    if (!m_connected) {
        return false;
    }
    
    // Home Assistant auto-discovery topic format
    // homeassistant/binary_sensor/esp32_smart_home/config
    char topic[256];
    snprintf(topic, sizeof(topic), "homeassistant/binary_sensor/%s/config", device_id);
    
    // Home Assistant discovery payload
    char payload[512];
    snprintf(payload, sizeof(payload),
        "{"
        "\"name\":\"%s\","
        "\"unique_id\":\"%s\","
        "\"device_class\":\"connectivity\","
        "\"state_topic\":\"smart_home/%s/state\","
        "\"availability_topic\":\"smart_home/%s/availability\","
        "\"payload_on\":\"online\","
        "\"payload_off\":\"offline\""
        "}",
        device_name, device_id, device_id, device_id);
    
    ESP_LOGI(TAG, "Publishing HA discovery: %s", topic);
    return publish(topic, payload, strlen(payload));
}

bool MqttService::subscribe(const char* topic, int qos) {
    if (!m_connected || m_client == nullptr || topic == nullptr) {
        return false;
    }
    
    // Use QoS 0 for subscriptions too (as per requirements)
    int msg_id = esp_mqtt_client_subscribe(m_client, topic, 0);
    if (msg_id < 0) {
        ESP_LOGE(TAG, "Failed to subscribe to topic: %s", topic);
        return false;
    }
    
    ESP_LOGI(TAG, "Subscribed to %s (msg_id: %d, QoS 0)", topic, msg_id);

    publishEvent(EventType::MQTT_SUBSCRIBED);

    return true;
}

void MqttService::stop() {
    if (m_task_handle != nullptr) {
        vTaskDelete(m_task_handle);
        m_task_handle = nullptr;
    }
    
    if (m_client != nullptr) {
        esp_mqtt_client_stop(m_client);
        esp_mqtt_client_destroy(m_client);
        m_client = nullptr;
    }
    
    m_initialized = false;
    m_connected = false;
}

void MqttService::taskLoop() {
    ESP_LOGI(TAG, "MQTT task started on Core %d", xPortGetCoreID());
    
    // Task can handle periodic operations or monitoring
    while (true) {
        // Feed watchdog periodically
        if (WatchdogSupervisor::getInstance()) {
            WatchdogSupervisor::getInstance()->feedWatchdog(WatchdogTask::MQTT_SERVICE);
        }
        vTaskDelay(pdMS_TO_TICKS(5000));  // Check every 5 seconds
        
        // Could add periodic health checks here
    }
}

void MqttService::taskEntry(void* parameter) {
    MqttService* service = static_cast<MqttService*>(parameter);
    service->taskLoop();
}

void MqttService::mqttEventHandler(void* handler_args, esp_event_base_t base,
                                  int32_t event_id, void* event_data) {
    MqttService* service = static_cast<MqttService*>(handler_args);
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT connected");
            service->m_connected = true;
            service->publishEvent(EventType::MQTT_CONNECTED);
            break;
            
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT disconnected");
            service->m_connected = false;
            service->publishEvent(EventType::MQTT_DISCONNECTED);
            break;
            
        case MQTT_EVENT_DATA:
            // esp-mqtt's topic/data point into its own buffer and are valid only
            // for the duration of this callback; they are not NUL-terminated.
            // They deliberately do not travel through the EventBus, whose
            // payload is a fixed-size union copied by value -- copying these
            // into stack locals and queueing pointers to them was a
            // use-after-free.
            ESP_LOGI(TAG, "MQTT data on %.*s: %.*s",
                     event->topic_len, event->topic,
                     event->data_len, event->data);
            // Phase 2 replaces this with a command callback.
            break;
        
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT error");
            service->m_connected = false;
            ErrorHandler::getInstance().reportError(
                ErrorCategory::MQTT_ERROR,
                event->error_handle->esp_transport_sock_errno,
                "MQTT error occurred"
            );
            service->publishEvent(EventType::MQTT_ERROR);
            break;
            
        default:
            break;
    }
}

void MqttService::publishEvent(EventType type, const EventPayload& payload) {
    EventMessage event;
    event.type = type;
    event.source = EventSource::MQTT_SERVICE;
    event.destination = EventSource::APPLICATION;
    event.payload = payload;
    
    EventBus::getInstance().publish(event);
}

