/**
 * @file WifiService.h
 * @brief WiFi Service (Core 0)
 * 
 * Manages WiFi STA connection with auto-reconnect.
 * Runs in dedicated FreeRTOS task pinned to Core 0.
 */

#pragma once

#include "core/eventbus/EventBus.h"
#include "services/wifi/WifiConfigService.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string>

#define WIFI_SSID_MAX_LEN     32
#define WIFI_PASS_MAX_LEN     64

/**
 * @brief WiFi Service
 * 
 * Event-driven WiFi management with auto-reconnect
 * Publishes WiFi events to EventBus
 */
 class WifiService {
public:
    WifiService();
    ~WifiService();
    
    /**
     * @brief Initialize and start WiFi service
     * @return true on success
     */
    bool initialize();
    
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
     * @brief Get task handle (for wifi)
     */
    TaskHandle_t getTaskHandle() const { return m_task_handle; }


    /**
     * @brief FreeRTOS task entry point
     */
    static void taskEntry(void* parameter);


private:
    void taskLoop();
    void publishEvent(EventType type, const EventPayload& payload = {});
    
    // Event handlers
    static void onWifiEvent(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data);
    
    TaskHandle_t m_task_handle;
    esp_netif_t* m_netif;
    bool m_initialized;
    bool m_connected;
    bool m_should_reconnect;
    uint32_t m_reconnect_attempts;
    
    static const char* TAG;
    static constexpr int TASK_STACK_SIZE = 4096;
    static constexpr int TASK_PRIORITY = 5;
    static constexpr BaseType_t TASK_CORE = 0;  // Core 0
    typedef struct {
        char ssid[WIFI_SSID_MAX_LEN];
        char password[WIFI_PASS_MAX_LEN];
    } WifiConfigInfo_st; 

    WifiConfigInfo_st g_wifi_cfg = {
        .ssid = CONFIG_WIFI_SSID,
        .password = CONFIG_WIFI_PASSWORD
    };

};

