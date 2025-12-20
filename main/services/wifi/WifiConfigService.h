/**
 * @file WifiConfigService.h
 * @brief WiFi configuration service using NVS
 * 
 * Thread-safe access to WiFi credentials stored in NVS
 */

#pragma once

#include <string>
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"

/**
 * @brief WiFi Configuration Service
 * 
 * Manages WiFi credentials in NVS with thread-safe access
 */
class WifiConfigService {
public:
    static WifiConfigService& getInstance();
    
    /**
     * @brief Initialize NVS and load credentials
     * @return true on success
     */
    bool initialize();
    
    /**
     * @brief Get SSID from NVS
     * @param ssid Output buffer
     * @param max_len Maximum buffer length
     * @return true if SSID found
     */
    bool getSSID(char* ssid, size_t max_len);
    
    /**
     * @brief Get password from NVS
     * @param password Output buffer
     * @param max_len Maximum buffer length
     * @return true if password found
     */
    bool getPassword(char* password, size_t max_len);
    
    /**
     * @brief Set SSID in NVS
     * @param ssid SSID string
     * @return true on success
     */
    bool setSSID(const char* ssid);
    
    /**
     * @brief Set password in NVS
     * @param password Password string
     * @return true on success
     */
    bool setPassword(const char* password);
    
    /**
     * @brief Check if credentials are configured
     * @return true if both SSID and password exist
     */
    bool hasCredentials();
    
    /**
     * @brief Deinitialize
     */
    void deinitialize();

private:
    WifiConfigService() = default;
    ~WifiConfigService() = default;
    WifiConfigService(const WifiConfigService&) = delete;
    WifiConfigService& operator=(const WifiConfigService&) = delete;
    
    nvs_handle_t m_nvs_handle;
    SemaphoreHandle_t m_mutex;
    bool m_initialized;
    
    static const char* TAG;
    static const char* NVS_NAMESPACE;
    static const char* KEY_SSID;
    static const char* KEY_PASSWORD;
};

