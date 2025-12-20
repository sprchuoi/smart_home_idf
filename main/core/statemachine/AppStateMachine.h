/**
 * @file AppStateMachine.h
 * @brief Application State Machine (Core 1)
 * 
 * Central coordinator for application state transitions.
 * Receives events via EventBus and manages state.
 */

#pragma once

#include "core/eventbus/EventBus.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string>

/**
 * @brief Application states
 */
enum class AppState : uint8_t {
    INIT,
    WIFI_CONNECTING,
    WIFI_CONNECTED,
    MQTT_CONNECTING,
    RUNNING,
    OTA_UPDATING,
    ERROR,
    SLEEP
};

/**
 * @brief Application State Machine
 * 
 * Central coordinator that manages application state transitions
 * based on events from various services
 */
class AppStateMachine {
public:
    AppStateMachine();
    ~AppStateMachine();
    
    /**
     * @brief Initialize state machine
     * @return true on success
     */
    bool initialize();
    
    /**
     * @brief Get current state
     * @return Current state
     */
    AppState getState() const { return m_current_state; }
    
    /**
     * @brief Get current state as string
     * @return State name
     */
    std::string getStateString() const;
    
    /**
     * @brief Get task handle (for watchdog)
     */
    TaskHandle_t getTaskHandle() const { return m_task_handle; }
    
    /**
     * @brief Stop state machine
     */
    void stop();
    
    /**
     * @brief FreeRTOS task entry point
     */
    static void taskEntry(void* parameter);

private:
    void taskLoop();
    void handleEvent(const EventMessage& event);
    void transitionTo(AppState new_state);
    void onStateEnter(AppState state);
    void onStateExit(AppState state);
    
    TaskHandle_t m_task_handle;
    AppState m_current_state;
    bool m_initialized;
    
    static const char* TAG;
    static constexpr int TASK_STACK_SIZE = 4096;
    static constexpr int TASK_PRIORITY = 4;
    static constexpr BaseType_t TASK_CORE = 1;  // Core 1
};

