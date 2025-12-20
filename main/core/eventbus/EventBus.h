/**
 * @file EventBus.h
 * @brief Central Event Bus for inter-service communication
 * 
 * Thread-safe publish/subscribe event system using FreeRTOS queues.
 * All services communicate through this bus to maintain loose coupling.
 */

#pragma once

#include <functional>
#include <vector>
#include <memory>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "uart/cfg/Uart_cfg.hpp"

/**
 * @brief Event types in the system
 */
enum class EventType : uint8_t {
    // WiFi Events
    WIFI_STARTED,
    WIFI_CONNECTED,
    WIFI_DISCONNECTED,
    WIFI_GOT_IP,
    WIFI_LOST_IP,
    WIFI_ERROR,
    
    // MQTT Events
    MQTT_CONNECTING,
    MQTT_CONNECTED,
    MQTT_DISCONNECTED,
    MQTT_PUBLISHED,
    MQTT_SUBSCRIBED,
    MQTT_DATA_RECEIVED,
    MQTT_ERROR,
    
    // Wake Word Events
    WAKE_WORD_DETECTED,
    
    // State Machine Events
    STATE_CHANGED,
    
    // Display Events
    DISPLAY_UPDATE_REQUEST,
    DISPLAY_ERROR,
    
    // Audio Events
    AUDIO_STARTED,
    AUDIO_STOPPED,
    AUDIO_ERROR,
    
    // OTA Events
    OTA_STARTED,
    OTA_PROGRESS,
    OTA_COMPLETED,
    OTA_FAILED,
    
    // Power Events
    POWER_MODE_CHANGED,
    WAKE_UP,
    
    // UART Events
    UART_DATA_RECEIVED,
    
    // System Events
    SYSTEM_ERROR,
    SYSTEM_SHUTDOWN
};

/**
 * @brief Event source identifiers
 */
enum class EventSource : uint8_t {
    WIFI_SERVICE,
    MQTT_SERVICE,
    WAKE_WORD_SERVICE,
    STATE_MACHINE,
    DISPLAY,
    APPLICATION,
    ERROR_HANDLER,
    AUDIO_PIPELINE,
    OTA_SERVICE,
    POWER_MANAGER,
    UART_DRIVER
};

/**
 * @brief Event payload union
 */
union EventPayload {
    struct {
        uint32_t ip_addr;
        uint32_t netmask;
        uint32_t gateway;
    } wifi_ip_info;
    
    struct {
        const char* topic;
        const char* data;
        size_t data_len;
    } mqtt_data;
    
    struct {
        int error_code;
        char error_msg[128];
    } error_info;
    
    struct {
        const char* state_name;
    } state_info;
    
    struct {
        uint32_t progress_percent;
        const char* status;
    } ota_info;
    
    struct {
        uint8_t data[UART_RX_BUF_SIZE];
        size_t  data_len;
    } uart_data;
    
    struct {
        uint8_t mode;
    } power_info;
    
    uint32_t raw_data;
};

/**
 * @brief Event message structure
 */
struct EventMessage {
    EventType type;
    EventSource source;
    EventSource destination;  // EventSource::APPLICATION for broadcast
    EventPayload payload;
    uint32_t timestamp;
};

/**
 * @brief Event subscriber callback type
 */
using EventCallback = std::function<void(const EventMessage&)>;

/**
 * @brief Event Bus class
 * 
 * Thread-safe event bus using FreeRTOS queue.
 * Supports publish/subscribe pattern for decoupled communication.
 */
class EventBus {
public:
    static EventBus& getInstance();
    
    /**
     * @brief Initialize the event bus
     * @param queue_size Maximum number of events in queue
     * @return true on success
     */
    bool initialize(size_t queue_size = 50);
    
    /**
     * @brief Publish an event to the bus
     * @param event Event message to publish
     * @return true if published successfully
     */
    bool publish(const EventMessage& event);

    /**
     * @brief Reset the event queue
     * @return true on success
     */
    bool reset();
    
    /**
     * @brief Subscribe to events of a specific type
     * @param event_type Event type to subscribe to
     * @param callback Callback function to invoke
     * @return Subscription ID (for future unsubscribe)
     */
    size_t subscribe(EventType event_type, EventCallback callback);
    
    /**
     * @brief Process events (call from subscriber task)
     * Blocks until event is available
     */
    void processEvents();
    
    /**
     * @brief Get queue handle for direct access (advanced usage)
     */
    QueueHandle_t getQueueHandle() const { return m_queue; }
    
    /**
     * @brief Deinitialize and cleanup
     */
    void deinitialize();

private:
    EventBus() = default;
    ~EventBus() = default;
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;
    
    QueueHandle_t m_queue;
    SemaphoreHandle_t m_mutex;
    
    struct Subscription {
        EventType type;
        EventCallback callback;
    };
    std::vector<Subscription> m_subscriptions;
    
    static const char* TAG;
};

