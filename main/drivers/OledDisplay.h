/**
 * @file OledDisplay.h
 * @brief OLED Display Driver (Core 1)
 * 
 * SSD1306 OLED display via I2C.
 * Thread-safe rendering via queue-based updates.
 */

#pragma once

#include "core/EventBus.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include <string>

/**
 * @brief Display update message
 */
struct DisplayUpdate {
    enum Type {
        CLEAR,
        TEXT,
        WIFI_STATUS,
        MQTT_STATUS,
        APP_STATE
    } type;
    
    std::string line1;
    std::string line2;
    std::string line3;
    std::string line4;
};

/**
 * @brief OLED Display Driver
 * 
 * SSD1306 128x64 display with thread-safe updates
 */
class OledDisplay {
public:
    OledDisplay();
    ~OledDisplay();
    
    /**
     * @brief Initialize display
     * @param sda_pin SDA GPIO pin
     * @param scl_pin SCL GPIO pin
     * @param i2c_addr I2C address (default 0x3C)
     * @return true on success
     */
    bool initialize(int sda_pin = 21, int scl_pin = 22, uint8_t i2c_addr = 0x3C);
    
    /**
     * @brief Queue display update (non-blocking)
     * @param update Display update message
     * @return true if queued successfully
     */
    bool updateDisplay(const DisplayUpdate& update);
    
    /**
     * @brief Clear display
     */
    void clear();
    
    /**
     * @brief Stop display service
     */
    void stop();
    
    /**
     * @brief FreeRTOS task entry point
     */
    static void taskEntry(void* parameter);

private:
    void taskLoop();
    void renderUpdate(const DisplayUpdate& update);
    void writeCommand(uint8_t cmd);
    void writeData(uint8_t* data, size_t len);
    void initDisplay();
    
    i2c_master_bus_handle_t m_i2c_bus;
    i2c_master_dev_handle_t m_i2c_dev;
    TaskHandle_t m_task_handle;
    QueueHandle_t m_update_queue;
    SemaphoreHandle_t m_mutex;
    bool m_initialized;
    uint8_t m_i2c_addr;
    
    static const char* TAG;
    static constexpr int TASK_STACK_SIZE = 4096;
    static constexpr int TASK_PRIORITY = 2;
    static constexpr BaseType_t TASK_CORE = 1;  // Core 1
    static constexpr size_t QUEUE_SIZE = 5;
};

