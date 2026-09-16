/**
 * @file MqttConfigInterface.h
 * @brief MQTT broker and device identity configuration
 *
 * Stores broker coordinates and this node's identity in NVS, and exposes them
 * over the serial console. Deliberately modelled on WifiConfigInterface so
 * there is one provisioning pattern rather than two.
 */

#pragma once

#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_console.h"
#include "cfg/Mqtt_cfg.hpp"

class MqttConfigInterface {
public:
    static MqttConfigInterface& getInstance();

    /**
     * @brief Open NVS and register the console commands.
     */
    bool initialize();

    void deinitialize();

    /**
     * @brief Load stored configuration.
     *
     * @param cfg Destination, zero-initialised on failure.
     * @return true if a broker host is configured; false otherwise.
     */
    bool hasConfig(MqttConfigInfo_st* cfg);

    bool setHost(const char* host, uint16_t port);
    bool setCredentials(const char* user, const char* password);
    bool setDevice(const char* device_id, const char* name, const char* room);
    bool setOtaUrl(const char* url);
    bool clear();

    void printStatus();

private:
    MqttConfigInterface() = default;
    ~MqttConfigInterface() = default;
    MqttConfigInterface(const MqttConfigInterface&) = delete;
    MqttConfigInterface& operator=(const MqttConfigInterface&) = delete;

    bool registerConsoleCommands();

    /// Read every key into cfg, filling defaults for anything absent.
    void load(MqttConfigInfo_st* cfg);

    /// True if `id` is safe to use as an MQTT topic segment and HA identifier.
    static bool isValidDeviceId(const char* id);

    static int consoleSet(int argc, char** argv);
    static int consoleAuth(int argc, char** argv);
    static int consoleDevice(int argc, char** argv);
    static int consoleOtaUrl(int argc, char** argv);
    static int consoleStatus(int argc, char** argv);
    static int consoleClear(int argc, char** argv);

    nvs_handle_t m_nvs_handle;
    SemaphoreHandle_t m_mutex;
    bool m_initialized;

    static const char* TAG;
    static const char* NVS_NAMESPACE;
    static const char* KEY_HOST;
    static const char* KEY_PORT;
    static const char* KEY_USER;
    static const char* KEY_PASS;
    static const char* KEY_DEVID;
    static const char* KEY_NAME;
    static const char* KEY_ROOM;
    static const char* KEY_OTA_URL;
};
