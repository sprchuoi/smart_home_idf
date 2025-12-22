/**
 * @file WifiConfigInterface.h
 * @brief Unified WiFi Configuration and Provisioning Interface
 * 
 * Combines configuration (NVS storage) and provisioning (console commands)
 * into a single, optimized interface.
 */

#pragma once

#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_console.h"
#include "cfg/Wifi_cfg.hpp"

/**
 * @brief Unified WiFi Configuration Interface
 * 
 * Manages WiFi credentials in NVS with thread-safe access and console provisioning.
 */
class WifiConfigInterface {
public:
    static WifiConfigInterface& getInstance();
    
    /**
     * @brief Initialize WiFi configuration (NVS and console commands)
     * @return true on success
     */
    bool initialize();
    
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
     * @brief Set WiFi credentials programmatically
     * @param ssid WiFi SSID
     * @param password WiFi password
     * @return true on success
     */
    bool setCredentials(const char* ssid, const char* password);
    
    /**
     * @brief Check if credentials are configured
     * @return true if credentials exist
     */
    bool hasCredentials();
    
    /**
     * @brief Check if credentials are configured (overload)
     * @param wifi_cfg Pointer to WifiConfigInfo_st structure
     * @return true if credentials exist
     */
    bool hasCredentials(WifiConfigInfo_st* wifi_cfg);
    
    /**
     * @brief Clear stored credentials
     * @return true on success
     */
    bool clearCredentials();
    
    /**
     * @brief Print current WiFi status
     */
    void printStatus();
    
    /**
     * @brief Set default credentials if none exist
     * @param ssid Default SSID
     * @param password Default password
     * @return true if defaults were set, false if credentials already exist
     */
    bool setDefaultCredentials(const char* ssid, const char* password);
    
    /**
     * @brief Deinitialize
     */
    void deinitialize();

private:
    WifiConfigInterface() = default;
    ~WifiConfigInterface() = default;
    WifiConfigInterface(const WifiConfigInterface&) = delete;
    WifiConfigInterface& operator=(const WifiConfigInterface&) = delete;
    
    bool registerConsoleCommands();
    
    // Console command handlers
    static int consoleSetSSID(int argc, char **argv);
    static int consoleSetPassword(int argc, char **argv);
    static int consoleSetBoth(int argc, char **argv);
    static int consoleStatus(int argc, char **argv);
    static int consoleClear(int argc, char **argv);
    
    nvs_handle_t m_nvs_handle;
    SemaphoreHandle_t m_mutex;
    bool m_initialized;
    
    static const char* TAG;
    static const char* NVS_NAMESPACE;
    static const char* KEY_SSID;
    static const char* KEY_PASSWORD;
};
