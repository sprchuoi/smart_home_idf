/**
 * @file Application.h
 * @brief Main Application orchestrator
 * 
 * Initializes and coordinates all services
 */

#pragma once


#include "core/eventbus/EventBus.h"
#include "core/statemachine/AppStateMachine.h"
#include "core/audio/AudioStateMachine.h"
#include "core/powermanager/PowerManager.h"
#include "core/watchdog/WatchdogSupervisor.h"
#include "services/wifi/WifiService.h"
#include "services/wifi/WifiConfigService.h"
#include "services/wifi/WifiProvisioning.h"
#include "services/mqtt/MqttService.h"
#include "services/wakeup/WakeWordService.h"
#include "services/audio/AudioPipeline.h"
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

    void setupEventSubscriptions();
    void handleStateChange(const EventMessage& event);
    void handleWakeWord(const EventMessage& event);
    
    WifiService m_wifi_service;
    MqttService m_mqtt_service;
    WakeWordService m_wake_word_service;
    AudioPipeline m_audio_pipeline;
    OTAService m_ota_service;
    OledDisplay m_display;
    UartDriver m_uart_driver;
    AppStateMachine m_state_machine;
    AudioStateMachine m_audio_state_machine;
    PowerManager m_power_manager;
    WatchdogSupervisor m_watchdog;
    
    bool m_initialized;
    bool m_running;
    
    static const char* TAG;
};

