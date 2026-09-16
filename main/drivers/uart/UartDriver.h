/**
 * @file UartDriver.h
 * @brief UART Driver (Core 0)
 * 
 * UART RX interrupt-driven communication.
 * ISR-safe queue-based data handling.
 */

#pragma once

#include "esp_log.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <cstdint>

#include "cfg/Uart_cfg.hpp"
/**
 * @brief UART Driver
 * 
 * Interrupt-driven UART RX with queue-based data handling
 */
class UartDriver {
public:
    UartDriver();
    ~UartDriver();
    
    /**
     * @brief Initialize UART driver
     * @param uart_num UART number (UART_NUM_0, UART_NUM_1, etc.)
     * @param baud_rate Baud rate
     * @param tx_pin TX GPIO pin
     * @param rx_pin RX GPIO pin
     * @param rx_buffer_size RX buffer size
     * @return true on success
     */
    bool initialize(uart_port_t uart_num = UART_NUM_0,
                   int baud_rate = 115200,
                   gpio_num_t tx_pin = GPIO_NUM_1,
                   gpio_num_t rx_pin = GPIO_NUM_3,
                   size_t rx_buffer_size = 1024);
    
    /**
     * @brief Start UART driver
     * @return true on success
     */
    bool start();
    
    /**
     * @brief Stop UART driver
     */
    void stop();
    
    /**
     * @brief Read data from UART
     * @param buffer Output buffer
     * @param buffer_size Buffer size
     * @param timeout_ms Timeout in milliseconds
     * @return Number of bytes read
     */
    size_t read(uint8_t* buffer, size_t buffer_size, uint32_t timeout_ms = 100);
    
    /**
     * @brief Write data to UART
     * @param data Data to write
     * @param data_len Data length
     * @return Number of bytes written
     */
    size_t write(const uint8_t* data, size_t data_len);
    
    /**
     * @brief FreeRTOS task entry point
     */
    static void taskEntry(void* parameter);

private:
    void taskLoop();
    static void uartIsrHandler(void* arg);
    
    uart_port_t m_uart_num;
    QueueHandle_t m_uart_queue;
    TaskHandle_t m_task_handle;
    bool m_initialized;
    bool m_running;
    
    static const char* TAG;
};

