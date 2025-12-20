/**
 * @file MqttService.h
 * @brief MQTT Service (Core 0)
 * 
 * Async MQTT client using esp-mqtt.
 * Runs in dedicated FreeRTOS task pinned to Core 0.
 */

#pragma once

#include "core/EventBus.h"
#include "mqtt_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string>

/**
 * @brief MQTT Service
 * 
 * Async MQTT client with auto-reconnect
 * Publishes MQTT events to EventBus
 */
class MqttService {
public:
    MqttService();
    ~MqttService();
    
    /**
     * @brief Initialize MQTT service
     * @param broker_uri MQTT broker URI (e.g., "mqtt://192.168.1.100:1883")
     * @param client_id Client ID
     * @return true on success
     */
    bool initialize(const char* broker_uri, const char* client_id);
    
    /**
     * @brief Connect to MQTT broker
     * @return true on success
     */
    bool connect();
    
    /**
     * @brief Disconnect from MQTT broker
     */
    void disconnect();
    
    /**
     * @brief Check if connected
     * @return true if connected
     */
    bool isConnected() const { return m_connected; }
    
    /**
     * @brief Get task handle (for watchdog)
     */
    TaskHandle_t getTaskHandle() const { return m_task_handle; }
    
    /**
     * @brief Publish message (QoS 0 only for Home Assistant)
     * @param topic Topic string
     * @param data Data payload
     * @param data_len Data length
     * @return true on success
     */
    bool publish(const char* topic, const char* data, size_t data_len);
    
    /**
     * @brief Publish Home Assistant auto-discovery message
     * @param device_name Device name
     * @param device_id Device unique ID
     * @return true on success
     */
    bool publishHADiscovery(const char* device_name, const char* device_id);
    
    /**
     * @brief Subscribe to topic
     * @param topic Topic string
     * @param qos QoS level (0-2)
     * @return true on success
     */
    bool subscribe(const char* topic, int qos = 1);
    
    /**
     * @brief Stop MQTT service
     */
    void stop();
    
    /**
     * @brief FreeRTOS task entry point
     */
    static void taskEntry(void* parameter);

private:
    void taskLoop();
    void publishEvent(EventType type, const EventPayload& payload = {});
    
    // MQTT event handler
    static void mqttEventHandler(void* handler_args, esp_event_base_t base,
                                 int32_t event_id, void* event_data);
    
    esp_mqtt_client_handle_t m_client;
    TaskHandle_t m_task_handle;
    bool m_initialized;
    bool m_connected;
    std::string m_broker_uri;
    std::string m_client_id;
    
    static const char* TAG;
    static constexpr int TASK_STACK_SIZE = 4096;
    static constexpr int TASK_PRIORITY = 5;
    static constexpr BaseType_t TASK_CORE = 0;  // Core 0
};

