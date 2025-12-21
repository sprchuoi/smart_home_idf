/**
 * @file WifiProvisioning.h
 * @brief WiFi provisioning service for setting up credentials
 * 
 * Provides multiple ways to configure WiFi credentials:
 * - Console commands
 * - Programmatic API
 * - Default fallback values
 */

#pragma once

#include <string>
#include "esp_log.h"
#include "esp_console.h"
#include "WifiConfigService.h"

/**
 * @brief WiFi Provisioning Service
 * 
 * Handles WiFi credential setup through various methods
 */
class WifiProvisioning {
public:
    /**
     * @brief Get singleton instance
     */
    static WifiProvisioning& getInstance();
    
    /**
     * @brief Initialize provisioning service
     * @return true on success
     */
    bool initialize();
    
    /**
     * @brief Register console commands for WiFi configuration
     * @return true on success
     */
    bool registerConsoleCommands();
    
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

private:
    WifiProvisioning() = default;
    ~WifiProvisioning() = default;
    WifiProvisioning(const WifiProvisioning&) = delete;
    WifiProvisioning& operator=(const WifiProvisioning&) = delete;
    
    bool m_initialized = false;
    
    static const char* TAG;
    
    // Console command handlers
    static int consoleSetSSID(int argc, char **argv);
    static int consoleSetPassword(int argc, char **argv);
    static int consoleSetBoth(int argc, char **argv);
    static int consoleStatus(int argc, char **argv);
    static int consoleClear(int argc, char **argv);
};
