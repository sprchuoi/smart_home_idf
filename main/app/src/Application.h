/**
 * @file Application.h
 * @brief Main Application orchestrator
 *
 * Owns the services and wires their callbacks together. Services no longer
 * talk through an EventBus: every edge here is 1:1 and single-listener, so
 * callbacks are both simpler and type-safe.
 */

#pragma once

#include "core/statemachine/AppStateMachine.h"
#include "services/wifi/WifiService.h"
#include "services/wifi/WifiConfigInterface.h"
#include "services/mqtt/MqttService.h"
#include "services/mqtt/MqttConfigInterface.h"
#include "services/ota/OTAService.h"
#include "drivers/uart/UartDriver.h"
#include "error/ErrorHandler.h"

#include <cstddef>

class Application {
public:
    Application();
    ~Application();

    /**
     * @brief Initialize application
     * @return true on success
     */
    bool initialize();

    /**
     * @brief Start application
     * @return true on success
     */
    bool start();

    /**
     * @brief Run application (blocks)
     */
    void run();

    /**
     * @brief Stop application
     */
    void stop();

private:
    /// WiFi link-state changes. Drives MQTT start/stop and the app state.
    void onWifiEvent(const WifiQueueEvent& event);

    /// Broker connection changes. Runs in the esp-mqtt task.
    void onMqttConnection(bool connected);

    /// Commands arriving on smart_home/<id>/cmd/<target>.
    /// Runs in the esp-mqtt task: must not block.
    void onMqttCommand(const char* topic, size_t topic_len,
                       const char* data, size_t data_len);

    /// Publish link diagnostics so the pipeline is observable end to end.
    void publishTelemetry();

    WifiService m_wifi_service;
    MqttService m_mqtt_service;
    OTAService m_ota_service;
    UartDriver m_uart_driver;
    AppStateMachine m_state_machine;

    bool m_initialized = false;
    bool m_running = false;

    WifiConfigInfo_st m_wifi_cfg = {};
    MqttConfigInfo_st m_mqtt_cfg = {};

    static const char* TAG;
};
