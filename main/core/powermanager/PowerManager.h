/**
 * @file PowerManager.h
 * @brief Power Management (Core 1)
 * 
 * Manages power modes, wake-up sources, and sleep transitions.
 */

#pragma once

#include "core/eventbus/EventBus.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_sleep.h"
#include <string>

/**
 * @brief Power modes
 */
enum class PowerMode : uint8_t {
    NORMAL,
    MODEM_SLEEP,
    LIGHT_SLEEP
};

/**
 * @brief Wake-up sources
 */
enum class WakeSource : uint8_t {
    VOICE,
    UART,
    MQTT,
    TIMER,
    UNKNOWN
};

/**
 * @brief Power Manager
 * 
 * Manages power modes and wake-up sources
 */
class PowerManager {
public:
    PowerManager();
    ~PowerManager();
    
    /**
     * @brief Initialize power manager
     * @return true on success
     */
    bool initialize();
    
    /**
     * @brief Get current power mode
     * @return Current power mode
     */
    PowerMode getPowerMode() const { return m_current_mode; }
    
    /**
     * @brief Request power mode change
     * @param mode Target power mode
     * @return true on success
     */
    bool requestPowerMode(PowerMode mode);
    
    /**
     * @brief Get last wake-up source
     * @return Wake-up source
     */
    WakeSource getWakeSource() const { return m_last_wake_source; }
    
    /**
     * @brief Check if system can enter sleep
     * @return true if sleep is allowed
     */
    bool canEnterSleep() const;
    
    /**
     * @brief Enter light sleep
     * @param duration_ms Sleep duration in milliseconds (0 = indefinite)
     * @return Wake-up source
     */
    WakeSource enterLightSleep(uint32_t duration_ms = 0);
    
    /**
     * @brief Stop power manager
     */
    void stop();
    
    /**
     * @brief FreeRTOS task entry point
     */
    static void taskEntry(void* parameter);

private:
    void taskLoop();
    void handleEvent(const EventMessage& event);
    void configureWakeSources();
    void suspendServices();
    void resumeServices();
    
    TaskHandle_t m_task_handle;
    PowerMode m_current_mode;
    WakeSource m_last_wake_source;
    bool m_initialized;
    bool m_sleep_blocked;
    EventGroupHandle_t m_sleep_event_group;
    
    // Event group bits
    static constexpr int BIT_WIFI_ACTIVE = BIT0;
    static constexpr int BIT_MQTT_ACTIVE = BIT1;
    static constexpr int BIT_AUDIO_ACTIVE = BIT2;
    static constexpr int BIT_OTA_ACTIVE = BIT3;
    
    static const char* TAG;
    static constexpr int TASK_STACK_SIZE = 4096;
    static constexpr int TASK_PRIORITY = 3;
    static constexpr BaseType_t TASK_CORE = 1;  // Core 1
};

