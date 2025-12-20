/**
 * @file WakeWordService.h
 * @brief Wake Word Service (Core 1)
 * 
 * ESP-SR WakeNet integration for wake word detection.
 * Uses AudioPipeline for audio input.
 */

#pragma once

#include "core/EventBus.h"
#include "services/AudioPipeline.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstdint>

// ESP-SR includes (uncomment when ESP-SR is available)
// #include "esp_wn_iface.h"
// #include "esp_wn_models.h"

/**
 * @brief Wake Word Service
 * 
 * Detects wake words using ESP-SR WakeNet
 * Processes audio from AudioPipeline
 */
class WakeWordService {
public:
    WakeWordService();
    ~WakeWordService();
    
    /**
     * @brief Initialize wake word service
     * @param audio_pipeline Pointer to AudioPipeline instance
     * @return true on success
     */
    bool initialize(AudioPipeline* audio_pipeline);
    
    /**
     * @brief Start wake word detection
     * @return true on success
     */
    bool start();
    
    /**
     * @brief Stop wake word detection
     */
    void stop();
    
    /**
     * @brief Get task handle (for watchdog)
     */
    TaskHandle_t getTaskHandle() const { return m_task_handle; }
    
    /**
     * @brief FreeRTOS task entry point
     */
    static void taskEntry(void* parameter);

private:
    void taskLoop();
    void processAudioData(const uint8_t* audio_data, size_t data_len);
    void publishWakeWordEvent();
    bool initializeESP_SR();
    
    AudioPipeline* m_audio_pipeline;
    TaskHandle_t m_task_handle;
    bool m_initialized;
    bool m_running;
    
    // ESP-SR handles (uncomment when ESP-SR is available)
    // esp_wn_iface_t* m_wakenet_iface;
    // model_coeff_getter_t* m_model_coeff_getter;
    // void* m_wakenet_detect;
    
    static const char* TAG;
    static constexpr int TASK_STACK_SIZE = 8192;
    static constexpr int TASK_PRIORITY = 3;
    static constexpr BaseType_t TASK_CORE = 1;  // Core 1
    static constexpr size_t AUDIO_BUFFER_SIZE = 512;
    static constexpr const char* WAKE_WORD = "Hey ESP";
};

