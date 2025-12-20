/**
 * @file AudioPipeline.h
 * @brief Audio Pipeline (Core 1)
 * 
 * Full I2S DMA audio capture pipeline with ESP-SR integration.
 * Zero-copy ring buffer implementation.
 */

#pragma once

#include "core/eventbus/EventBus.h"
#include "core/audio/AudioStateMachine.h"
#include "driver/i2s_std.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include <cstdint>
#include <memory>

/**
 * @brief Audio Pipeline
 * 
 * I2S DMA audio capture with ring buffer
 */
class AudioPipeline {
public:
    AudioPipeline();
    ~AudioPipeline();
    
    /**
     * @brief Initialize audio pipeline
     * @param sample_rate Sample rate (e.g., 16000)
     * @param bits_per_sample Bits per sample (16 or 32)
     * @param i2s_num I2S peripheral number
     * @param bck_pin BCLK pin
     * @param ws_pin WS pin
     * @param din_pin Data input pin
     * @return true on success
     */
    bool initialize(uint32_t sample_rate = 16000,
                   uint8_t bits_per_sample = 16,
                   i2s_port_t i2s_num = I2S_NUM_0,
                   gpio_num_t bck_pin = GPIO_NUM_4,
                   gpio_num_t ws_pin = GPIO_NUM_5,
                   gpio_num_t din_pin = GPIO_NUM_18);
    
    /**
     * @brief Start audio capture
     * @return true on success
     */
    bool start();
    
    /**
     * @brief Stop audio capture
     */
    void stop();
    
    /**
     * @brief Get audio data from ring buffer
     * @param buffer Output buffer
     * @param buffer_size Buffer size in bytes
     * @param timeout_ms Timeout in milliseconds
     * @return Number of bytes read, or 0 on error
     */
    size_t readAudioData(uint8_t* buffer, size_t buffer_size, uint32_t timeout_ms = 100);
    
    /**
     * @brief Check if pipeline is running
     * @return true if running
     */
    bool isRunning() const { return m_running; }
    
    /**
     * @brief Get task handle (for watchdog)
     */
    TaskHandle_t getTaskHandle() const { return m_task_handle; }
    
    /**
     * @brief Suspend audio pipeline (for sleep)
     */
    void suspend();
    
    /**
     * @brief Resume audio pipeline (after wake)
     */
    void resume();
    
    /**
     * @brief FreeRTOS task entry point
     */
    static void taskEntry(void* parameter);

private:
    void taskLoop();
    void i2sIsrHandler();
    static void i2sIsrHandlerStatic(void* arg);
    
    i2s_chan_handle_t m_rx_handle;
    RingbufHandle_t m_ring_buffer;
    TaskHandle_t m_task_handle;
    bool m_initialized;
    bool m_running;
    bool m_suspended;
    uint32_t m_sample_rate;
    uint8_t m_bits_per_sample;
    
    // DMA buffers
    static constexpr size_t DMA_BUFFER_COUNT = 8;
    static constexpr size_t DMA_BUFFER_SIZE = 1024;
    static constexpr size_t RING_BUFFER_SIZE = 8192;
    
    static const char* TAG;
    static constexpr int TASK_STACK_SIZE = 8192;
    static constexpr int TASK_PRIORITY = 5;
    static constexpr BaseType_t TASK_CORE = 1;  // Core 1
};

