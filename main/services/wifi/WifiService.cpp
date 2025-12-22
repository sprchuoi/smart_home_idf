/**
 * @file WifiService.cpp
 * @brief WiFi Service implementation - Optimized for minimal stack usage
 */

#include "WifiService.h"
#include "error/ErrorHandler.h"
#include <cstring>

const char* WifiService::TAG = "WifiService";

WifiService::WifiService()
    : m_task_handle(nullptr)
    , m_netif(nullptr)
    , m_event_queue(nullptr)
    , m_initialized(false)
    , m_connected(false)
    , m_should_reconnect(false)
    , m_reconnect_attempts(0) {
}

WifiService::~WifiService() {
    stop();
}

bool WifiService::initialize(WifiConfigInfo_st *m_wifi_cfg) {
    if (m_initialized) {
        return true;
    }
    esp_err_t err = ESP_OK;
    
    // Store WiFi config
    if (m_wifi_cfg != nullptr) {
        memcpy(&this->m_wifi_cfg, m_wifi_cfg, sizeof(WifiConfigInfo_st));
    }
    
    // Create queue for WiFi events (minimal items)
    m_event_queue = xQueueCreate(EVENT_QUEUE_SIZE, sizeof(WifiQueueEvent));
    if (m_event_queue == nullptr) {
        ESP_LOGE(TAG, "Failed to create event queue");
        return false;
    }
    
    // Initialize event loop (only once)
    static bool event_loop_initialized = false;
    if (!event_loop_initialized) {
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        event_loop_initialized = true;
    }
    
    // Initialize network interface
    m_netif = esp_netif_create_default_wifi_sta();
    if (m_netif == nullptr) {
        ESP_LOGE(TAG, "Failed to create WiFi STA netif");
        return false;
    }
    
    // Initialize WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_init failed: %s", esp_err_to_name(err));
        return false;
    }
    
    // Set WiFi mode
    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_mode failed: %s", esp_err_to_name(err));
        return false;
    }
    
    // Register event handlers - minimal processing in handlers
    err = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                             &onWifiEvent, this, &m_wifi_event_inst);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register WiFi handler: %s", esp_err_to_name(err));
        return false;
    }
    
    err = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                             &onWifiEvent, this, &m_ip_event_inst);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to register IP handler: %s", esp_err_to_name(err));
    }
    
    // Create task - all heavy work happens here
    BaseType_t result = xTaskCreatePinnedToCore(
        taskEntry, "WifiService", TASK_STACK_SIZE, this, TASK_PRIORITY,
        &m_task_handle, TASK_CORE
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create WiFi task");
        return false;
    }
    
    m_initialized = true;
    vTaskDelay(pdMS_TO_TICKS(50));
    m_should_reconnect = true;
    
    ESP_LOGI(TAG, "WifiService initialized");
    return true;
}

bool WifiService::connect() {
    if (!m_initialized) {
        return false;
    }
    
    if (strlen(m_wifi_cfg.ssid) == 0) {
        ESP_LOGW(TAG, "SSID not configured");
        return false;
    }
    
    // Configure WiFi - minimal stack variables
    wifi_config_t cfg = {};
    strncpy((char*)cfg.sta.ssid, m_wifi_cfg.ssid, sizeof(cfg.sta.ssid) - 1);
    strncpy((char*)cfg.sta.password, m_wifi_cfg.password, sizeof(cfg.sta.password) - 1);
    cfg.sta.threshold.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_config failed");
        return false;
    }

    err = esp_wifi_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_start failed");
        return false;
    }

    ESP_LOGI(TAG, "WiFi connection initiated");
    return true;
}

void WifiService::disconnect() {
    m_should_reconnect = false;
    if (m_initialized) {
        esp_wifi_disconnect();
    }
}

void WifiService::stop() {
    if (m_task_handle != nullptr) {
        vTaskDelete(m_task_handle);
        m_task_handle = nullptr;
    }
    
    if (m_event_queue != nullptr) {
        vQueueDelete(m_event_queue);
        m_event_queue = nullptr;
    }
    
    if (m_initialized) {
        esp_wifi_stop();
        esp_wifi_deinit();
        m_initialized = false;
    }
    
    m_connected = false;
}

std::string WifiService::getIPAddress() const {
    if (!m_connected || m_netif == nullptr) {
        return "";
    }
    
    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(m_netif, &ip_info) == ESP_OK) {
        char ip_str[16];
        esp_ip4addr_ntoa(&ip_info.ip, ip_str, sizeof(ip_str));
        return std::string(ip_str);
    }
    
    return "";
}

// Minimal event handler - only queues events
void WifiService::onWifiEvent(void* arg, esp_event_base_t event_base,
                             int32_t event_id, void* event_data) {
    if (arg == nullptr) return;
    
    WifiService* service = static_cast<WifiService*>(arg);
    if (service->m_event_queue == nullptr) return;
    
    WifiQueueEvent queue_event = {};
    
    // Minimal processing - just extract needed data
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGD(TAG, "WIFI_EVENT_STA_START");
                queue_event.type = WifiEventType::STA_START;
                break;
            case WIFI_EVENT_STA_DISCONNECTED: {
                ESP_LOGD(TAG, "WIFI_EVENT_STA_DISCONNECTED");
                queue_event.type = WifiEventType::DISCONNECTED;
                if (event_data) {
                    wifi_event_sta_disconnected_t* disc = 
                        (wifi_event_sta_disconnected_t*)event_data;
                    queue_event.reason = disc->reason;
                }
                break;
            }
            default:
                return;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        queue_event.type = WifiEventType::GOT_IP;
        if (event_data) {
            ip_event_got_ip_t* ip = (ip_event_got_ip_t*)event_data;
            queue_event.ip_addr = ip->ip_info.ip.addr;
        }
    } else {
        return;
    }
    
    // Post to queue - non-blocking
    xQueueSendFromISR(service->m_event_queue, &queue_event, nullptr);
}

void WifiService::handleWifiEvent(const WifiQueueEvent& event) {
    switch (event.type) {
        case WifiEventType::STA_START:
            m_should_reconnect = true;
            break;
            
        case WifiEventType::GOT_IP:
            m_connected = true;
            m_reconnect_attempts = 0;
            m_should_reconnect = false;
            
            // Publish event - minimal EventPayload
            {
                EventPayload payload = {};
                payload.wifi_ip_info.ip_addr = event.ip_addr;
                publishEvent(EventType::WIFI_CONNECTED, payload);
            }
            break;
            
        case WifiEventType::DISCONNECTED:
            m_connected = false;
            m_should_reconnect = true;
            publishEvent(EventType::WIFI_DISCONNECTED);
            break;
    }
}

void WifiService::taskLoop() {
    WifiQueueEvent event;
    TickType_t last_reconnect = 0;
    
    while (true) {
        // Check queue for events with timeout
        if (xQueueReceive(m_event_queue, &event, pdMS_TO_TICKS(1000))) {
            handleWifiEvent(event);
        }
        
        // Handle reconnection logic
        if (m_should_reconnect && !m_connected && m_initialized) {
            TickType_t now = xTaskGetTickCount();
            
            if ((now - last_reconnect) > pdMS_TO_TICKS(5000) && m_reconnect_attempts < 10) {
                last_reconnect = now;
                m_reconnect_attempts++;
                
                ESP_LOGW(TAG, "Reconnect attempt %lu", m_reconnect_attempts);
                esp_wifi_connect();
            } else if (m_reconnect_attempts >= 10) {
                m_should_reconnect = false;
                ESP_LOGW(TAG, "Max reconnection attempts reached");
            }
        }
    }
}

void WifiService::taskEntry(void* parameter) {
    WifiService* service = static_cast<WifiService*>(parameter);
    service->taskLoop();
}

void WifiService::publishEvent(EventType type, const EventPayload& payload) {
    EventMessage event;
    event.type = type;
    event.source = EventSource::WIFI_SERVICE;
    event.destination = EventSource::APPLICATION;
    event.payload = payload;
    
    EventBus::getInstance().publish(event);
}