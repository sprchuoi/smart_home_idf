/**
 * @file OledDisplay.cpp
 * @brief OLED Display Driver implementation
 */

#include "OledDisplay.h"
#include "error/ErrorHandler.h"
#include <cstring>

const char* OledDisplay::TAG = "OledDisplay";

// SSD1306 commands
#define SSD1306_SETCONTRAST 0x81
#define SSD1306_DISPLAYALLON_RESUME 0xA4
#define SSD1306_DISPLAYALLON 0xA5
#define SSD1306_NORMALDISPLAY 0xA6
#define SSD1306_INVERTDISPLAY 0xA7
#define SSD1306_DISPLAYOFF 0xAE
#define SSD1306_DISPLAYON 0xAF
#define SSD1306_SETDISPLAYOFFSET 0xD3
#define SSD1306_SETCOMPINS 0xDA
#define SSD1306_SETVCOMDETECT 0xDB
#define SSD1306_SETDISPLAYCLOCKDIV 0xD5
#define SSD1306_SETPRECHARGE 0xD9
#define SSD1306_SETMULTIPLEX 0xA8
#define SSD1306_SETLOWCOLUMN 0x00
#define SSD1306_SETHIGHCOLUMN 0x10
#define SSD1306_SETSTARTLINE 0x40
#define SSD1306_MEMORYMODE 0x20
#define SSD1306_COLUMNADDR 0x21
#define SSD1306_PAGEADDR 0x22
#define SSD1306_COMSCANINC 0xC0
#define SSD1306_COMSCANDEC 0xC8
#define SSD1306_SEGREMAP 0xA0
#define SSD1306_CHARGEPUMP 0x8D
#define SSD1306_EXTERNALVCC 0x1
#define SSD1306_SWITCHCAPVCC 0x2

OledDisplay::OledDisplay()
    : m_i2c_bus(nullptr)
    , m_i2c_dev(nullptr)
    , m_task_handle(nullptr)
    , m_update_queue(nullptr)
    , m_mutex(nullptr)
    , m_initialized(false)
    , m_i2c_addr(0x3C) {
}

OledDisplay::~OledDisplay() {
    stop();
}

bool OledDisplay::initialize(int sda_pin, int scl_pin, uint8_t i2c_addr) {
    if (m_initialized) {
        return true;
    }
    
    m_i2c_addr = i2c_addr;
    
    // Create mutex
    m_mutex = xSemaphoreCreateMutex();
    if (m_mutex == nullptr) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return false;
    }
    
    // Create update queue
    m_update_queue = xQueueCreate(QUEUE_SIZE, sizeof(DisplayUpdate));
    if (m_update_queue == nullptr) {
        ESP_LOGE(TAG, "Failed to create update queue");
        vSemaphoreDelete(m_mutex);
        m_mutex = nullptr;
        return false;
    }
    
    // Initialize I2C bus
    i2c_master_bus_config_t i2c_bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = (gpio_num_t)sda_pin,
        .scl_io_num = (gpio_num_t)scl_pin,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags = {
            .enable_internal_pullup = true,
        },
    };
    
    // Temporarily reduce I2C log level to suppress NACK errors when display not connected
    esp_log_level_t old_level = esp_log_level_get("i2c.master");
    esp_log_level_set("i2c.master", ESP_LOG_NONE);
    
    esp_err_t err = i2c_new_master_bus(&i2c_bus_config, &m_i2c_bus);
    
    esp_log_level_set("i2c.master", old_level);
    
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to create I2C bus (display may not be connected)");
        vQueueDelete(m_update_queue);
        vSemaphoreDelete(m_mutex);
        m_update_queue = nullptr;
        m_mutex = nullptr;
        return false;
    }
    
    // Add I2C device
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = m_i2c_addr,
        .scl_speed_hz = 100000,
    };
    
    esp_log_level_set("i2c.master", ESP_LOG_NONE);
    err = i2c_master_bus_add_device(m_i2c_bus, &dev_cfg, &m_i2c_dev);
    esp_log_level_set("i2c.master", old_level);
    
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to add I2C device (display not connected)");
        i2c_del_master_bus(m_i2c_bus);
        m_i2c_bus = nullptr;
        vQueueDelete(m_update_queue);
        vSemaphoreDelete(m_mutex);
        m_update_queue = nullptr;
        m_mutex = nullptr;
        return false;
    }
    
    // Initialize display hardware
    initDisplay();
    
    // Create task on Core 1
    BaseType_t result = xTaskCreatePinnedToCore(
        taskEntry,
        "OledDisplay",
        TASK_STACK_SIZE,
        this,
        TASK_PRIORITY,
        &m_task_handle,
        TASK_CORE
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create display task");
        i2c_master_bus_rm_device(m_i2c_dev);
        i2c_del_master_bus(m_i2c_bus);
        m_i2c_bus = nullptr;
        m_i2c_dev = nullptr;
        vQueueDelete(m_update_queue);
        vSemaphoreDelete(m_mutex);
        m_update_queue = nullptr;
        m_mutex = nullptr;
        return false;
    }
    
    m_initialized = true;
    ESP_LOGI(TAG, "OledDisplay initialized (Core %d, I2C: 0x%02X)", TASK_CORE, m_i2c_addr);
    return true;
}

bool OledDisplay::updateDisplay(const DisplayUpdate& update) {
    if (!m_initialized || m_update_queue == nullptr) {
        return false;
    }
    
    // Copy update to queue (non-blocking)
    BaseType_t result = xQueueSend(m_update_queue, &update, 0);
    if (result != pdTRUE) {
        ESP_LOGW(TAG, "Display update queue full");
        return false;
    }
    
    return true;
}

void OledDisplay::clear() {
    DisplayUpdate update;
    update.type = DisplayUpdate::CLEAR;
    updateDisplay(update);
}

void OledDisplay::stop() {
    if (m_task_handle != nullptr) {
        vTaskDelete(m_task_handle);
        m_task_handle = nullptr;
    }
    
    if (m_update_queue != nullptr) {
        vQueueDelete(m_update_queue);
        m_update_queue = nullptr;
    }
    
    if (m_mutex != nullptr) {
        vSemaphoreDelete(m_mutex);
        m_mutex = nullptr;
    }
    
    if (m_i2c_dev != nullptr) {
        i2c_master_bus_rm_device(m_i2c_dev);
        m_i2c_dev = nullptr;
    }
    
    if (m_i2c_bus != nullptr) {
        i2c_del_master_bus(m_i2c_bus);
        m_i2c_bus = nullptr;
    }
    
    m_initialized = false;
}

void OledDisplay::taskEntry(void* parameter) {
    static_cast<OledDisplay*>(parameter)->taskLoop();
    vTaskDelete(NULL); // safety net (never reached)
}

void OledDisplay::taskLoop() {
    ESP_LOGI(TAG, "OLED display task started on Core %d", xPortGetCoreID());
    
    DisplayUpdate update;
    
    while (true) {
        // Wait for display update
        if (xQueueReceive(m_update_queue, &update, portMAX_DELAY) == pdTRUE) {
            renderUpdate(update);
        }
    }
}

void OledDisplay::renderUpdate(const DisplayUpdate& update) {
    if (xSemaphoreTake(m_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to acquire display mutex");
        // should not return here to avoid blocking updates
    }
    
    switch (update.type) {
        case DisplayUpdate::CLEAR:
            // Clear display (simplified - would need full implementation)
            writeCommand(SSD1306_DISPLAYOFF);
            writeCommand(SSD1306_DISPLAYON);
            break;
            
        case DisplayUpdate::TEXT:
        case DisplayUpdate::WIFI_STATUS:
        case DisplayUpdate::MQTT_STATUS:
        case DisplayUpdate::APP_STATE:
            // For now, just log the update
            // Full implementation would render text to display
            ESP_LOGI(TAG, "Display update: %s / %s / %s / %s",
                    update.line1.c_str(),
                    update.line2.c_str(),
                    update.line3.c_str(),
                    update.line4.c_str());
            break;
    }
    
    xSemaphoreGive(m_mutex);
}

void OledDisplay::writeCommand(uint8_t cmd) {
    uint8_t buffer[2] = {0x00, cmd};  // Control byte + command
    i2c_master_transmit(m_i2c_dev, buffer, 2, -1);
}

void OledDisplay::writeData(uint8_t* data, size_t len) {
    uint8_t* buffer = new uint8_t[len + 1];
    buffer[0] = 0x40;  // Control byte for data
    memcpy(buffer + 1, data, len);
    i2c_master_transmit(m_i2c_dev, buffer, len + 1, -1);
    delete[] buffer;
    vTaskDelete(nullptr);  // REQUIRED
}

void OledDisplay::initDisplay() {
    // Initialize SSD1306 display
    writeCommand(SSD1306_DISPLAYOFF);
    writeCommand(SSD1306_SETDISPLAYCLOCKDIV);
    writeCommand(0x80);
    writeCommand(SSD1306_SETMULTIPLEX);
    writeCommand(0x3F);
    writeCommand(SSD1306_SETDISPLAYOFFSET);
    writeCommand(0x0);
    writeCommand(SSD1306_SETSTARTLINE | 0x0);
    writeCommand(SSD1306_CHARGEPUMP);
    writeCommand(0x14);
    writeCommand(SSD1306_MEMORYMODE);
    writeCommand(0x00);
    writeCommand(SSD1306_SEGREMAP | 0x1);
    writeCommand(SSD1306_COMSCANDEC);
    writeCommand(SSD1306_SETCOMPINS);
    writeCommand(0x12);
    writeCommand(SSD1306_SETCONTRAST);
    writeCommand(0xCF);
    writeCommand(SSD1306_SETPRECHARGE);
    writeCommand(0xF1);
    writeCommand(SSD1306_SETVCOMDETECT);
    writeCommand(0x40);
    writeCommand(SSD1306_DISPLAYALLON_RESUME);
    writeCommand(SSD1306_NORMALDISPLAY);
    writeCommand(SSD1306_DISPLAYON);
    
    ESP_LOGI(TAG, "SSD1306 display initialized");
}

