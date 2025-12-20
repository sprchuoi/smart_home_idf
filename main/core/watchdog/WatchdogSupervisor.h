/**
 * @file WatchdogSupervisor.h
 * @brief Watchdog Supervisor (Core 1)
 * 
 * Monitors task heartbeats and triggers safe reset on failure.
 */

#pragma once

#include "core/eventbus/EventBus.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_task_wdt.h"
#include <string>
#include <vector>

/**
 * @brief Task identifier for watchdog
 */
enum class WatchdogTask : uint8_t {
    WIFI_SERVICE = 0,
    MQTT_SERVICE = 1,
    APP_STATE_MACHINE = 2,
    AUDIO_PIPELINE = 3,
    WAKE_WORD_SERVICE = 4,
    MAX_TASKS = 5
};

/**
 * @brief Watchdog Supervisor
 * 
 * Monitors critical tasks and triggers reset on timeout
 */
class WatchdogSupervisor {
public:
    WatchdogSupervisor();
    ~WatchdogSupervisor();
    
    /**
     * @brief Initialize watchdog supervisor
     * @param timeout_seconds Watchdog timeout in seconds
     * @return true on success
     */
    bool initialize(const esp_task_wdt_config_t wdt_config = {});
    
    /**
     * @brief Register a task for monitoring
     * @param task_id Task identifier
     * @param task_handle FreeRTOS task handle
     * @return true on success
     */
    bool registerTask(WatchdogTask task_id, TaskHandle_t task_handle);
    
    /**
     * @brief Feed watchdog for a specific task
     * @param task_id Task identifier
     * @return true on success
     */
    bool feedWatchdog(WatchdogTask task_id);
    
    /**
     * @brief Get task heartbeat status
     * @param task_id Task identifier
     * @return true if task is alive
     */
    bool isTaskAlive(WatchdogTask task_id) const;
    
    /**
     * @brief Stop watchdog supervisor
     */
    void stop();
    
    /**
     * @brief FreeRTOS task entry point
     */
    static void taskEntry(void* parameter);

    /**
     * @brief Set watchdog timeout for all tasks
     * @param timeout_seconds Timeout in seconds
     */
    void setWatchdogTimeout(uint32_t timeout_seconds);

    // Global instance accessors (used by services to feed supervisor)
    static void setInstance(WatchdogSupervisor* instance);
    static WatchdogSupervisor* getInstance();
    // Static instance storage (definition in cpp)
    static WatchdogSupervisor* s_instance;

private:
    void taskLoop();
    void checkTaskHealth();
    void triggerSafeReset(const char* reason);
    
    TaskHandle_t m_task_handle;
    EventGroupHandle_t m_heartbeat_group;
    bool m_initialized;
    uint32_t m_timeout_seconds;
    std::vector<TaskHandle_t> m_registered_tasks;
    std::vector<TickType_t> m_last_heartbeat;
    
    // Heartbeat bits (one per task)
    static constexpr int BIT_WIFI = BIT0;
    static constexpr int BIT_MQTT = BIT1;
    static constexpr int BIT_STATE_MACHINE = BIT2;
    static constexpr int BIT_AUDIO = BIT3;
    static constexpr int BIT_WAKE_WORD = BIT4;
    
    static const char* TAG;
    static constexpr int TASK_STACK_SIZE = 4096;
    static constexpr int TASK_PRIORITY = 2;
    static constexpr BaseType_t TASK_CORE = 1;  // Core 1
};

