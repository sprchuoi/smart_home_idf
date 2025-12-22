/**
 * @file WifiService.h
 * @brief WiFi Service (Core 0)
 * 
 * Manages WiFi STA connection with auto-reconnect.
 * Runs in dedicated FreeRTOS task pinned to Core 0.
 */

#pragma once

#include "core/eventbus/EventBus.h"
#include "services/wifi/WifiConfigInterface.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <cstring>

// Minimal event structure to reduce stack usage
enum class WifiEventType : uint8_t {
    STA_START = 0,
    GOT_IP = 1,
    DISCONNECTED = 2
};

struct WifiQueueEvent {
    WifiEventType type;
    uint32_t ip_addr;
    uint8_t reason;  // Disconnect reason
};

/**
 * @brief WiFi Service
 * 
 * Event-driven WiFi management with auto-reconnect
 * Publishes WiFi events to EventBus
 * Optimized for minimal stack usage
 */
class WifiService {
public:
    WifiService();
    ~WifiService();
    
    /**
     * @brief Initialize and start WiFi service
     * @param g_wifi_cfg WiFi configuration information
     * @return true on success
     */
    bool initialize(WifiConfigInfo_st *g_wifi_cfg);
    
    /**
     * @brief Start WiFi connection
     * @return true on success
     */
    bool connect();
    
    /**
     * @brief Disconnect WiFi
     */
    void disconnect();
    
    /**
     * @brief Check if WiFi is connected
     * @return true if connected
     */
    bool isConnected() const { return m_connected; }
    
    /**
     * @brief Get current IP address
     * @return IP address string
     */
    std::string getIPAddress() const;
    
    /**
     * @brief Stop WiFi service
     */
    void stop();
    
    /**
     * @brief Get task handle
     */
    TaskHandle_t getTaskHandle() const { return m_task_handle; }

    /**
     * @brief FreeRTOS task entry point
     */
    static void taskEntry(void* parameter);

private:
    void taskLoop();
    void publishEvent(EventType type, const EventPayload& payload = {});
    void handleWifiEvent(const WifiQueueEvent& event);
    
    // Minimal event handler - only posts to queue
    static void onWifiEvent(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data);
    
    TaskHandle_t m_task_handle;
    esp_netif_t* m_netif;
    QueueHandle_t m_event_queue;
    bool m_initialized;
    bool m_connected;
    bool m_should_reconnect;
    uint32_t m_reconnect_attempts;
    
    static const char* TAG;
    static constexpr int TASK_STACK_SIZE = 3072;  // Reduced: minimal stack needed
    static constexpr int TASK_PRIORITY = 5;
    static constexpr BaseType_t TASK_CORE = 0;  // Core 0
    static constexpr int EVENT_QUEUE_SIZE = 5;

    WifiConfigInfo_st m_wifi_cfg;
    esp_event_handler_instance_t m_wifi_event_inst;
    esp_event_handler_instance_t m_ip_event_inst;
};


