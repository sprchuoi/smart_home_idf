/**
 * @file WakeWordService.cpp
 * @brief Wake Word Service implementation
 */

#include "WakeWordService.h"
#include "error/ErrorHandler.h"

const char* WakeWordService::TAG = "WakeWordService";

WakeWordService::WakeWordService()
    : m_audio_pipeline(nullptr)
    , m_task_handle(nullptr)
    , m_initialized(false)
    , m_running(false) {
}

WakeWordService::~WakeWordService() {
    stop();
}

bool WakeWordService::initialize(AudioPipeline* audio_pipeline) {
    if (m_initialized) {
        return true;
    }
    
    if (audio_pipeline == nullptr) {
        ESP_LOGE(TAG, "AudioPipeline is null");
        return false;
    }
    
    m_audio_pipeline = audio_pipeline;
    
    // Initialize ESP-SR (stub - uncomment when ESP-SR is available)
    if (!initializeESP_SR()) {
        ESP_LOGW(TAG, "ESP-SR initialization failed, using stub mode");
    }
    
    // Create task on Core 1
    BaseType_t result = xTaskCreatePinnedToCore(
        taskEntry,
        "WakeWordService",
        TASK_STACK_SIZE,
        this,
        TASK_PRIORITY,
        &m_task_handle,
        TASK_CORE
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create wake word task");
        return false;
    }
    
    m_initialized = true;
    ESP_LOGI(TAG, "WakeWordService initialized (Core %d, wake word: %s)", TASK_CORE, WAKE_WORD);
    return true;
}

bool WakeWordService::start() {
    if (!m_initialized) {
        ESP_LOGE(TAG, "WakeWordService not initialized");
        return false;
    }
    
    m_running = true;
    ESP_LOGI(TAG, "Wake word detection started");
    return true;
}

void WakeWordService::stop() {
    m_running = false;
    
    if (m_task_handle != nullptr) {
        vTaskDelete(m_task_handle);
        m_task_handle = nullptr;
    }
    
    m_initialized = false;
}

void WakeWordService::taskLoop() {
    ESP_LOGI(TAG, "Wake word task started on Core %d", xPortGetCoreID());
    
    uint8_t* audio_buffer = new uint8_t[AUDIO_BUFFER_SIZE];
    
    while (m_running) {
        if (m_audio_pipeline == nullptr || !m_audio_pipeline->isRunning()) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        
        // Read audio data from pipeline
        size_t bytes_read = m_audio_pipeline->readAudioData(audio_buffer, AUDIO_BUFFER_SIZE, 100);
        
        if (bytes_read > 0) {
            // Process audio data with ESP-SR WakeNet
            processAudioData(audio_buffer, bytes_read);
        }
    }
    
    delete[] audio_buffer;
}

void WakeWordService::processAudioData(const uint8_t* audio_data, size_t data_len) {
    // ESP-SR WakeNet processing (stub implementation)
    // In real implementation:
    // 1. Convert audio data to format expected by WakeNet
    // 2. Call wakenet_detect() with audio data
    // 3. Check if wake word detected
    
    // Stub: Simulate detection every ~3 seconds of audio (for demo)
    static uint32_t audio_samples_processed = 0;
    audio_samples_processed += data_len / 2;  // Assuming 16-bit samples
    
    // Approximate: 16000 samples/sec, detect every 3 seconds = 48000 samples
    if (audio_samples_processed >= 48000) {
        audio_samples_processed = 0;
        ESP_LOGI(TAG, "Wake word detected: %s", WAKE_WORD);
        publishWakeWordEvent();
    }
}

bool WakeWordService::initializeESP_SR() {
    // ESP-SR initialization (stub - implement when ESP-SR is available)
    // In real implementation:
    // 1. Load WakeNet model coefficients
    // 2. Initialize WakeNet interface
    // 3. Configure detection threshold
    
    ESP_LOGI(TAG, "ESP-SR initialization (stub mode)");
    return false;  // Return false to indicate stub mode
}

void WakeWordService::taskEntry(void* parameter) {
    WakeWordService* service = static_cast<WakeWordService*>(parameter);
    service->taskLoop();
}

void WakeWordService::publishWakeWordEvent() {
    EventMessage event;
    event.type = EventType::WAKE_WORD_DETECTED;
    event.source = EventSource::WAKE_WORD_SERVICE;
    event.destination = EventSource::APPLICATION;
    
    EventBus::getInstance().publish(event);
}

