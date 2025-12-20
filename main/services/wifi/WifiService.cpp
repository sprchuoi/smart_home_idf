/**
 * @file WifiService.cpp
 * @brief WiFi Service implementation
 */

#include "WifiService.h"
#include "error/ErrorHandler.h"
#include <cstring>
#include "core/watchdog/WatchdogSupervisor.h"

const char* WifiService::TAG = "WifiService";



WifiService::WifiService()
    : m_task_handle(nullptr)
    , m_netif(nullptr)
    , m_initialized(false)
    , m_connected(false)
    , m_should_reconnect(true)
    , m_reconnect_attempts(0) {
}

WifiService::~WifiService() {
    stop();
}

bool WifiService::initialize() {
    if (m_initialized) {
        return true;
    }
    // esp-netif
    ESP_ERROR_CHECK(esp_netif_init());
    //  Event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    // Initialize network interface
    m_netif = esp_netif_create_default_wifi_sta();
    if (m_netif == nullptr) {
        ESP_LOGE(TAG, "Failed to create default WiFi STA netif");
        return false;
    }
    assert(m_netif);
    
    // Initialize WiFi with default config
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t err = esp_wifi_init(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi: %s", esp_err_to_name(err));
        return false;
    }
    
    // Register event handlers
    err = esp_event_handler_instance_register(WIFI_EVENT,
                                             ESP_EVENT_ANY_ID,
                                             &onWifiEvent,
                                             this,
                                             nullptr);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register WiFi event handler: %s", esp_err_to_name(err));
        return false;
    }
    
    err = esp_event_handler_instance_register(IP_EVENT,
                                             IP_EVENT_STA_GOT_IP,
                                             &onWifiEvent,
                                             this,
                                             nullptr);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register IP event handler: %s", esp_err_to_name(err));
        return false;
    }
    
    // Set WiFi mode to STA
    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi mode: %s", esp_err_to_name(err));
        return false;
    }
    
    // Create task on Core 0
    BaseType_t result = xTaskCreatePinnedToCore(
        taskEntry,
        "WifiService",
        TASK_STACK_SIZE,
        this,
        TASK_PRIORITY,
        &m_task_handle,
        TASK_CORE
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create WiFi task");
        return false;
    }
    
    m_initialized = true;
    ESP_LOGI(TAG, "WifiService initialized (Core %d)", TASK_CORE);
    return true;
}

bool WifiService::connect() {
    if (!m_initialized) {
        ESP_LOGE(TAG, "WifiService not initialized");
        return false;
    }
    
    // Check if credentials are configured
    if (!WifiConfigService::getInstance().hasCredentials()) {
        ESP_LOGE(TAG, "WiFi credentials not configured!");
        ESP_LOGE(TAG, "Use console command: wifi_set <ssid> <password>");
        ErrorHandler::getInstance().reportError(
            ErrorCategory::WIFI_ERROR,
            ESP_ERR_NOT_FOUND,
            "WiFi credentials not configured"
        );
        return false;
    }
    
    // Get credentials from config service
    
    if (!WifiConfigService::getInstance().getSSID(g_wifi_cfg.ssid, sizeof(g_wifi_cfg.ssid))) {
        ESP_LOGE(TAG, "SSID not configured");
        ErrorHandler::getInstance().reportError(
            ErrorCategory::WIFI_ERROR,
            ESP_ERR_NOT_FOUND,
            "WiFi SSID not configured"
        );
        return false;
    }
    
    if (!WifiConfigService::getInstance().getPassword(g_wifi_cfg.password, sizeof(g_wifi_cfg.password))) {
        ESP_LOGE(TAG, "Password not configured");
        ErrorHandler::getInstance().reportError(
            ErrorCategory::WIFI_ERROR,
            ESP_ERR_NOT_FOUND,
            "WiFi password not configured"
        );
        return false;
    }
    
    // Configure WiFi
    wifi_config_t wifi_config = {};
    strncpy((char*)wifi_config.sta.ssid, g_wifi_cfg.ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char*)wifi_config.sta.password, g_wifi_cfg.password, sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    
    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi config: %s", esp_err_to_name(err));
        ErrorHandler::getInstance().reportError(
            ErrorCategory::WIFI_ERROR,
            err,
            "Failed to set WiFi config"
        );
        return false;
    }
    
    // Start WiFi
    err = esp_wifi_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WiFi: %s", esp_err_to_name(err));
        ErrorHandler::getInstance().reportError(
            ErrorCategory::WIFI_ERROR,
            err,
            "Failed to start WiFi"
        );
        return false;
    }
    
    publishEvent(EventType::WIFI_STARTED);
    
    // Begin connection
    err = esp_wifi_connect();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initiate WiFi connection: %s", esp_err_to_name(err));
        ErrorHandler::getInstance().reportError(
            ErrorCategory::WIFI_ERROR,
            err,
            "Failed to initiate WiFi connection"
        );
        return false;
    }
    
    m_should_reconnect = true;
    m_reconnect_attempts = 0;
    ESP_LOGI(TAG, "WiFi connection initiated to: %s", g_wifi_cfg.ssid);
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

void WifiService::taskLoop() {
    ESP_LOGI(TAG, "WiFi task started on Core %d", xPortGetCoreID());
    
    while (true) {
        // Task handles reconnection logic
        if (m_should_reconnect && !m_connected && m_initialized) {
            if (m_reconnect_attempts < 10) {
                // Feed watchdog while waiting to reconnect
                if (WatchdogSupervisor::getInstance()) {
                    WatchdogSupervisor::getInstance()->feedWatchdog(WatchdogTask::WIFI_SERVICE);
                }
                vTaskDelay(pdMS_TO_TICKS(5000));  // Wait 5 seconds
                ESP_LOGI(TAG, "Attempting WiFi reconnection (attempt %lu)", m_reconnect_attempts + 1);
                esp_wifi_connect();
                m_reconnect_attempts++;
            } else {
                ESP_LOGW(TAG, "Max reconnection attempts reached");
                m_should_reconnect = false;
            }
        }
        
        // Always feed watchdog at least once per loop and yield
        if (WatchdogSupervisor::getInstance()) {
            WatchdogSupervisor::getInstance()->feedWatchdog(WatchdogTask::WIFI_SERVICE);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));  // Check every second
    }
}

void WifiService::taskEntry(void* parameter) {
    WifiService* service = static_cast<WifiService*>(parameter);
    service->taskLoop();
}

void WifiService::onWifiEvent(void* arg, esp_event_base_t event_base,
                             int32_t event_id, void* event_data) {
    WifiService* service = static_cast<WifiService*>(arg);
    
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "WiFi STA started");
                service->publishEvent(EventType::WIFI_STARTED);
                break;
                
            case WIFI_EVENT_STA_CONNECTED: {
                wifi_event_sta_connected_t* event = (wifi_event_sta_connected_t*)event_data;
                ESP_LOGI(TAG, "WiFi connected to: %s", event->ssid);
                service->m_connected = true;
                service->m_reconnect_attempts = 0;
                service->publishEvent(EventType::WIFI_CONNECTED);
                break;
            }
            
            case WIFI_EVENT_STA_DISCONNECTED: {
                wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*)event_data;
                ESP_LOGW(TAG, "WiFi disconnected (reason: %d)", event->reason);
                service->m_connected = false;
                service->publishEvent(EventType::WIFI_DISCONNECTED);
                
                // Trigger reconnection in task loop
                break;
            }
            
            default:
                break;
        }
    } else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
            ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
            
            EventPayload payload;
            payload.wifi_ip_info.ip_addr = event->ip_info.ip.addr;
            payload.wifi_ip_info.netmask = event->ip_info.netmask.addr;
            payload.wifi_ip_info.gateway = event->ip_info.gw.addr;
            
            service->publishEvent(EventType::WIFI_GOT_IP, payload);
        }
    }
}

void WifiService::publishEvent(EventType type, const EventPayload& payload) {
    EventMessage event;
    event.type = type;
    event.source = EventSource::WIFI_SERVICE;
    event.destination = EventSource::APPLICATION;
    event.payload = payload;
    
    EventBus::getInstance().publish(event);
}

