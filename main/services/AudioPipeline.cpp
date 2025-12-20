/**
 * @file AudioPipeline.cpp
 * @brief Audio Pipeline implementation
 */

#include "AudioPipeline.h"
#include "error/ErrorHandler.h"
#include <cstring>

const char* AudioPipeline::TAG = "AudioPipeline";

AudioPipeline::AudioPipeline()
    : m_rx_handle(nullptr)
    , m_ring_buffer(nullptr)
    , m_task_handle(nullptr)
    , m_initialized(false)
    , m_running(false)
    , m_suspended(false)
    , m_sample_rate(16000)
    , m_bits_per_sample(16) {
}

AudioPipeline::~AudioPipeline() {
    stop();
}

bool AudioPipeline::initialize(uint32_t sample_rate,
                              uint8_t bits_per_sample,
                              i2s_port_t i2s_num,
                              gpio_num_t bck_pin,
                              gpio_num_t ws_pin,
                              gpio_num_t din_pin) {
    if (m_initialized) {
        return true;
    }
    
    m_sample_rate = sample_rate;
    m_bits_per_sample = bits_per_sample;
    
    // Create ring buffer
    m_ring_buffer = xRingbufferCreate(RING_BUFFER_SIZE, RINGBUF_TYPE_BYTEBUF);
    if (m_ring_buffer == nullptr) {
        ESP_LOGE(TAG, "Failed to create ring buffer");
        return false;
    }
    
    // Configure I2S
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(i2s_num, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = DMA_BUFFER_COUNT;
    chan_cfg.dma_frame_num = DMA_BUFFER_SIZE;
    
    esp_err_t err = i2s_new_channel(&chan_cfg, &m_rx_handle, nullptr);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2S channel: %s", esp_err_to_name(err));
        vRingbufferDelete(m_ring_buffer);
        m_ring_buffer = nullptr;
        return false;
    }
    
    // Configure I2S standard mode
    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = sample_rate,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
        },
        .slot_cfg = {
            .data_bit_width = (i2s_data_bit_width_t)bits_per_sample,
            .slot_bit_width = (i2s_slot_bit_width_t)bits_per_sample,
            .slot_mode = I2S_SLOT_MODE_MONO,
            .slot_mask = I2S_STD_SLOT_LEFT,
            .ws_width = (i2s_slot_bit_width_t)bits_per_sample,
            .ws_pol = false,
            .bit_shift = false,
            .msb_right = false,
        },
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = bck_pin,
            .ws = ws_pin,
            .dout = I2S_GPIO_UNUSED,
            .din = din_pin,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    
    err = i2s_channel_init_std_mode(m_rx_handle, &std_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init I2S std mode: %s", esp_err_to_name(err));
        i2s_del_channel(m_rx_handle);
        m_rx_handle = nullptr;
        vRingbufferDelete(m_ring_buffer);
        m_ring_buffer = nullptr;
        return false;
    }
    
    // Create task on Core 1
    BaseType_t result = xTaskCreatePinnedToCore(
        taskEntry,
        "AudioPipeline",
        TASK_STACK_SIZE,
        this,
        TASK_PRIORITY,
        &m_task_handle,
        TASK_CORE
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create audio pipeline task");
        i2s_del_channel(m_rx_handle);
        m_rx_handle = nullptr;
        vRingbufferDelete(m_ring_buffer);
        m_ring_buffer = nullptr;
        return false;
    }
    
    m_initialized = true;
    ESP_LOGI(TAG, "AudioPipeline initialized (Core %d, %lu Hz, %d bits)",
            TASK_CORE, sample_rate, bits_per_sample);
    return true;
}

bool AudioPipeline::start() {
    if (!m_initialized || m_running) {
        return false;
    }
    
    if (m_suspended) {
        resume();
    }
    
    esp_err_t err = i2s_channel_enable(m_rx_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable I2S channel: %s", esp_err_to_name(err));
        ErrorHandler::getInstance().reportError(
            ErrorCategory::AUDIO_ERROR,
            err,
            "Failed to enable I2S channel"
        );
        return false;
    }
    
    m_running = true;
    
    EventMessage event;
    event.type = EventType::AUDIO_STARTED;
    event.source = EventSource::AUDIO_PIPELINE;
    event.destination = EventSource::APPLICATION;
    
    EventBus::getInstance().publish(event);
    
    ESP_LOGI(TAG, "Audio pipeline started");
    return true;
}

void AudioPipeline::stop() {
    if (m_running) {
        i2s_channel_disable(m_rx_handle);
        m_running = false;
        
        EventMessage event;
        event.type = EventType::AUDIO_STOPPED;
        event.source = EventSource::AUDIO_PIPELINE;
        event.destination = EventSource::APPLICATION;
        
        EventBus::getInstance().publish(event);
    }
    
    if (m_task_handle != nullptr) {
        vTaskDelete(m_task_handle);
        m_task_handle = nullptr;
    }
    
    if (m_rx_handle != nullptr) {
        i2s_del_channel(m_rx_handle);
        m_rx_handle = nullptr;
    }
    
    if (m_ring_buffer != nullptr) {
        vRingbufferDelete(m_ring_buffer);
        m_ring_buffer = nullptr;
    }
    
    m_initialized = false;
}

size_t AudioPipeline::readAudioData(uint8_t* buffer, size_t buffer_size, uint32_t timeout_ms) {
    if (!m_running || m_ring_buffer == nullptr || buffer == nullptr) {
        return 0;
    }
    
    void* data = xRingbufferReceive(m_ring_buffer, &buffer_size, pdMS_TO_TICKS(timeout_ms));
    if (data == nullptr) {
        return 0;
    }
    
    memcpy(buffer, data, buffer_size);
    vRingbufferReturnItem(m_ring_buffer, data);
    
    return buffer_size;
}

void AudioPipeline::suspend() {
    if (m_running && !m_suspended) {
        i2s_channel_disable(m_rx_handle);
        m_suspended = true;
        ESP_LOGI(TAG, "Audio pipeline suspended");
    }
}

void AudioPipeline::resume() {
    if (m_suspended) {
        esp_err_t err = i2s_channel_enable(m_rx_handle);
        if (err == ESP_OK) {
            m_suspended = false;
            ESP_LOGI(TAG, "Audio pipeline resumed");
        } else {
            ESP_LOGE(TAG, "Failed to resume audio pipeline: %s", esp_err_to_name(err));
        }
    }
}

void AudioPipeline::taskLoop() {
    ESP_LOGI(TAG, "Audio pipeline task started on Core %d", xPortGetCoreID());
    
    uint8_t* dma_buffer = new uint8_t[DMA_BUFFER_SIZE];
    
    while (m_running) {
        if (m_suspended) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        
        // Read from I2S DMA
        size_t bytes_read = 0;
        esp_err_t err = i2s_channel_read(m_rx_handle, dma_buffer, DMA_BUFFER_SIZE, &bytes_read, portMAX_DELAY);
        
        if (err == ESP_OK && bytes_read > 0) {
            // Send to ring buffer (non-blocking)
            BaseType_t result = xRingbufferSend(m_ring_buffer, dma_buffer, bytes_read, 0);
            if (result != pdTRUE) {
                ESP_LOGW(TAG, "Ring buffer full, dropping audio data");
            }
        } else if (err != ESP_OK) {
            ESP_LOGE(TAG, "I2S read error: %s", esp_err_to_name(err));
            ErrorHandler::getInstance().reportError(
                ErrorCategory::AUDIO_ERROR,
                err,
                "I2S read error"
            );
        }
    }
    
    delete[] dma_buffer;
}

void AudioPipeline::taskEntry(void* parameter) {
    AudioPipeline* pipeline = static_cast<AudioPipeline*>(parameter);
    pipeline->taskLoop();
}

void AudioPipeline::i2sIsrHandler() {
    // ISR handler for I2S (if needed)
    // Currently using polling in task loop
}

void AudioPipeline::i2sIsrHandlerStatic(void* arg) {
    AudioPipeline* pipeline = static_cast<AudioPipeline*>(arg);
    pipeline->i2sIsrHandler();
}

