/**
 * @file Application.h
 * @brief Main Application orchestrator
 * 
 * Initializes and coordinates all services
 */

#pragma once


#include "core/eventbus/EventBus.h"
#include "core/statemachine/AppStateMachine.h"
#include "core/powermanager/PowerManager.h"
#include "core/watchdog/WatchdogSupervisor.h"
#include "services/wifi/WifiService.h"
#include "services/wifi/WifiConfigInterface.h"
#include "services/mqtt/MqttService.h"
#include "services/ota/OTAService.h"
#include "drivers/oled/OledDisplay.h"
#include "drivers/uart/UartDriver.h"
#include "error/ErrorHandler.h"


/**
 * @brief Main Application class
 * 
 * Orchestrates all services and manages application lifecycle
 */
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

    void handleStateChange(const EventMessage& event);

    WifiService m_wifi_service;
    MqttService m_mqtt_service;
    OTAService m_ota_service;
    OledDisplay m_display;
    UartDriver m_uart_driver;
    AppStateMachine m_state_machine;
    PowerManager m_power_manager;
    WatchdogSupervisor m_watchdog;
    
    bool m_initialized;
    bool m_running;
    
    WifiConfigInfo_st g_wifi_cfg = {
        .ssid = CONFIG_WIFI_SSID,
        .password = CONFIG_WIFI_PASSWORD
    };


    
    static const char* TAG;
};

