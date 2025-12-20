/**
 * @file UartDriver.cpp
 * @brief UART Driver implementation
 */

#include "UartDriver.h"
#include "error/ErrorHandler.h"
#include <cstring>

const char* UartDriver::TAG = "UartDriver";

UartDriver::UartDriver()
    : m_uart_num(UART_NUM_0)
    , m_uart_queue(nullptr)
    , m_task_handle(nullptr)
    , m_initialized(false)
    , m_running(false) {
}

UartDriver::~UartDriver() {
    stop();
}

bool UartDriver::initialize(uart_port_t uart_num,
                            int baud_rate,
                            gpio_num_t tx_pin,
                            gpio_num_t rx_pin,
                            size_t rx_buffer_size) {
    if (m_initialized) {
        return true;
    }
    
    m_uart_num = uart_num;
    
    // Configure UART
    uart_config_t uart_config = {
        .baud_rate = UART_DEFAULT_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .flags = 0
    };
    
    esp_err_t err = uart_driver_install(m_uart_num, UART_RX_BUF_SIZE, 0, 20, &m_uart_queue, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install UART driver: %s", esp_err_to_name(err));
        return false;
    }
    
    err = uart_param_config(m_uart_num, &uart_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure UART: %s", esp_err_to_name(err));
        uart_driver_delete(m_uart_num);
        return false;
    }
    
    err = uart_set_pin(m_uart_num, tx_pin, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set UART pins: %s", esp_err_to_name(err));
        uart_driver_delete(m_uart_num);
        return false;
    }
    
    // Enable UART interrupt
    err = uart_enable_rx_intr(m_uart_num);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable UART RX interrupt: %s", esp_err_to_name(err));
        uart_driver_delete(m_uart_num);
        return false;
    }
    
    // Create task on Core 0
    BaseType_t result = xTaskCreatePinnedToCore(
        taskEntry,
        "UartDriver",
        UART_TASK_STACK_SIZE,
        this,
        UART_TASK_PRIORITY,
        &m_task_handle,
        UART_TASK_CORE
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create UART task");
        uart_driver_delete(m_uart_num);
        return false;
    }
    
    m_initialized = true;
    ESP_LOGI(TAG, "UartDriver initialized (UART%d, %d baud, Core %d)",
            uart_num, baud_rate, UART_TASK_CORE);
    return true;
}

bool UartDriver::start() {
    if (!m_initialized) {
        return false;
    }
    
    m_running = true;
    ESP_LOGI(TAG, "UART driver started");
    return true;
}

void UartDriver::stop() {
    m_running = false;
    
    if (m_task_handle != nullptr) {
        vTaskDelete(m_task_handle);
        m_task_handle = nullptr;
    }
    
    if (m_uart_num != UART_NUM_MAX) {
        uart_disable_rx_intr(m_uart_num);
        uart_driver_delete(m_uart_num);
    }
    
    if (m_uart_queue != nullptr) {
        vQueueDelete(m_uart_queue);
        m_uart_queue = nullptr;
    }
    
    m_initialized = false;
}

size_t UartDriver::read(uint8_t* buffer, size_t buffer_size, uint32_t timeout_ms) {
    if (!m_running || buffer == nullptr || buffer_size == 0) {
        return 0;
    }
    
    int len = uart_read_bytes(m_uart_num, buffer, buffer_size, pdMS_TO_TICKS(timeout_ms));
    if (len > 0) {
        return static_cast<size_t>(len);
    }
    
    return 0;
}

size_t UartDriver::write(const uint8_t* data, size_t data_len) {
    if (!m_running || data == nullptr || data_len == 0) {
        return 0;
    }
    
    int len = uart_write_bytes(m_uart_num, data, data_len);
    return (len > 0) ? static_cast<size_t>(len) : 0;
}

void UartDriver::taskLoop()
{
    ESP_LOGI(TAG, "UART task started on Core %d", xPortGetCoreID());

    uart_event_t event;
    uint8_t buffer[UART_RX_BUF_SIZE];

    while (m_running) {
        if (xQueueReceive(m_uart_queue, &event, portMAX_DELAY) == pdTRUE) {
            switch (event.type) {

            case UART_DATA: {
                size_t len = read(buffer, UART_RX_BUF_SIZE, 0);
                if (len > 0) {
                    EventMessage uart_event{};
                    uart_event.type = EventType::UART_DATA_RECEIVED;
                    uart_event.source = EventSource::UART_DRIVER;
                    uart_event.destination = EventSource::APPLICATION;

                    memcpy(uart_event.payload.uart_data.data, buffer, len);
                    uart_event.payload.uart_data.data_len = len;

                    EventBus::getInstance().publish(uart_event);
                }
                break;
            }

            case UART_FIFO_OVF:
                ESP_LOGW(TAG, "UART FIFO overflow");
                uart_flush_input(m_uart_num);
                xQueueReset(m_uart_queue);
                break;

            case UART_BUFFER_FULL:
                ESP_LOGW(TAG, "UART buffer full");
                uart_flush_input(m_uart_num);
                xQueueReset(m_uart_queue);
                break;

            case UART_BREAK:
                ESP_LOGI(TAG, "UART RX break");
                break;

            case UART_PARITY_ERR:
                ESP_LOGI(TAG, "UART parity error");
                break;

            case UART_FRAME_ERR:
                ESP_LOGI(TAG, "UART frame error");
                break;

            case UART_DATA_BREAK:
                ESP_LOGI(TAG, "UART data break");
                break;

            case UART_PATTERN_DET:
                ESP_LOGI(TAG, "UART pattern detected");
                break;

            case UART_EVENT_MAX:
                ESP_LOGW(TAG, "UART event max");
                break;

            default:
                ESP_LOGW(TAG, "Unhandled UART event: %d", event.type);
                break;
            }
        }
    }
}

void UartDriver::taskEntry(void* parameter) {
    UartDriver* uart = static_cast<UartDriver*>(parameter);
    uart->taskLoop();
}

void UartDriver::uartIsrHandler(void* arg) {
    // ISR handler - events are queued automatically by UART driver
    (void)arg;
}

