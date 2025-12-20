/**
 * @file Application.cpp
 * @brief Main Application implementation
 */

#include "Application.h"
#include <string>
#include <cstring>
#include "nvs_flash.h"

const char* Application::TAG = "Application";

Application::Application()
    : m_initialized(false)
    , m_running(false) {
}

Application::~Application() {
    stop();
}

bool Application::initialize() {
    if (m_initialized) {
        return true;
    }
    
    ESP_LOGI(TAG, "Initializing Smart Home Application...");
    
    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition was truncated and needs to be erased");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    
    // Initialize EventBus
    if (!EventBus::getInstance().initialize()) {
        ESP_LOGE(TAG, "Failed to initialize EventBus");
        return false;
    }
    
    // Initialize WiFi Config Service
    if (!WifiConfigService::getInstance().initialize()) {
        ESP_LOGE(TAG, "Failed to initialize WifiConfigService");
        return false;
    }
    
    // Initialize Error Handler
    // (Singleton, no explicit init needed)
    
    // Initialize State Machine
    if (!m_state_machine.initialize()) {
        ESP_LOGE(TAG, "Failed to initialize AppStateMachine");
        return false;
    }
    
    // Initialize Display
    if (!m_display.initialize()) {
        ESP_LOGW(TAG, "Failed to initialize OLED display (continuing anyway)");
    }
    
    // Initialize WiFi Service
    if (!m_wifi_service.initialize()) {
        ESP_LOGE(TAG, "Failed to initialize WifiService");
        return false;
    }
    
    // Initialize MQTT Service
    // TODO: Configure MQTT broker URI and client ID
    const char* mqtt_broker = "mqtt://192.168.1.100:1883";  // Change to your broker
    const char* mqtt_client_id = "esp32_smart_home";
    
    if (!m_mqtt_service.initialize(mqtt_broker, mqtt_client_id)) {
        ESP_LOGE(TAG, "Failed to initialize MqttService");
        return false;
    }
    
    // Initialize Watchdog Supervisor
    if (!m_watchdog.initialize(30)) {  // 30 second timeout
        ESP_LOGE(TAG, "Failed to initialize WatchdogSupervisor");
        return false;
    }
    
    // Initialize Power Manager
    if (!m_power_manager.initialize()) {
        ESP_LOGE(TAG, "Failed to initialize PowerManager");
        return false;
    }
    
    // Initialize Audio State Machine
    if (!m_audio_state_machine.initialize()) {
        ESP_LOGE(TAG, "Failed to initialize AudioStateMachine");
        return false;
    }
    
    // Initialize Audio Pipeline
    if (!m_audio_pipeline.initialize(16000, 16, I2S_NUM_0, GPIO_NUM_4, GPIO_NUM_5, GPIO_NUM_18)) {
        ESP_LOGW(TAG, "Failed to initialize AudioPipeline (continuing anyway)");
    }
    
    // Initialize Wake Word Service (with AudioPipeline)
    if (!m_wake_word_service.initialize(&m_audio_pipeline)) {
        ESP_LOGW(TAG, "Failed to initialize WakeWordService (continuing anyway)");
    }
    
    // Initialize OTA Service
    if (!m_ota_service.initialize()) {
        ESP_LOGW(TAG, "Failed to initialize OTAService (continuing anyway)");
    }
    
    // Initialize UART Driver
    if (!m_uart_driver.initialize()) {
        ESP_LOGW(TAG, "Failed to initialize UartDriver (continuing anyway)");
    }
    
    // Register tasks with watchdog
    m_watchdog.registerTask(WatchdogTask::WIFI_SERVICE, m_wifi_service.getTaskHandle());
    m_watchdog.registerTask(WatchdogTask::MQTT_SERVICE, m_mqtt_service.getTaskHandle());
    m_watchdog.registerTask(WatchdogTask::APP_STATE_MACHINE, m_state_machine.getTaskHandle());
    m_watchdog.registerTask(WatchdogTask::AUDIO_PIPELINE, m_audio_pipeline.getTaskHandle());
    m_watchdog.registerTask(WatchdogTask::WAKE_WORD_SERVICE, m_wake_word_service.getTaskHandle());
    
    // Setup event subscriptions
    setupEventSubscriptions();
    
    m_initialized = true;
    ESP_LOGI(TAG, "Application initialized successfully");
    return true;
}

bool Application::start() {
    if (!m_initialized) {
        ESP_LOGE(TAG, "Application not initialized");
        return false;
    }
    
    if (m_running) {
        return true;
    }
    
    ESP_LOGI(TAG, "Starting application...");
    
    // Start WiFi connection
    if (!m_wifi_service.connect()) {
        ESP_LOGE(TAG, "Failed to start WiFi connection");
        return false;
    }
    
    // Start audio pipeline
    m_audio_pipeline.start();
    
    // Start wake word service
    m_wake_word_service.start();
    
    // Start UART driver
    m_uart_driver.start();
    
    // Register tasks with watchdog (after all tasks are created)
    TaskHandle_t wifi_handle = m_wifi_service.getTaskHandle();
    TaskHandle_t mqtt_handle = m_mqtt_service.getTaskHandle();
    TaskHandle_t state_handle = m_state_machine.getTaskHandle();
    TaskHandle_t audio_handle = m_audio_pipeline.getTaskHandle();
    TaskHandle_t wake_handle = m_wake_word_service.getTaskHandle();
    
    if (wifi_handle) m_watchdog.registerTask(WatchdogTask::WIFI_SERVICE, wifi_handle);
    if (mqtt_handle) m_watchdog.registerTask(WatchdogTask::MQTT_SERVICE, mqtt_handle);
    if (state_handle) m_watchdog.registerTask(WatchdogTask::APP_STATE_MACHINE, state_handle);
    if (audio_handle) m_watchdog.registerTask(WatchdogTask::AUDIO_PIPELINE, audio_handle);
    if (wake_handle) m_watchdog.registerTask(WatchdogTask::WAKE_WORD_SERVICE, wake_handle);
    
    // Publish Home Assistant discovery
    if (m_mqtt_service.isConnected()) {
        m_mqtt_service.publishHADiscovery("ESP32 Smart Home", "esp32_smart_home");
    }
    
    m_running = true;
    ESP_LOGI(TAG, "Application started");
    return true;
}

void Application::run() {
    if (!m_running) {
        if (!start()) {
            ESP_LOGE(TAG, "Failed to start application");
            return;
        }
    }
    
    ESP_LOGI(TAG, "Application running...");
    
    // Main loop - update display periodically
    while (m_running) {
        // Update display with current state
        DisplayUpdate update;
        update.type = DisplayUpdate::APP_STATE;
        update.line1 = "Smart Home";
        update.line2 = "State: " + m_state_machine.getStateString();
        update.line3 = "WiFi: " + std::string(m_wifi_service.isConnected() ? "Connected" : "Disconnected");
        update.line4 = "MQTT: " + std::string(m_mqtt_service.isConnected() ? "Connected" : "Disconnected");
        
        m_display.updateDisplay(update);
        
        vTaskDelay(pdMS_TO_TICKS(2000));  // Update every 2 seconds
    }
}

void Application::stop() {
    if (!m_running) {
        return;
    }
    
    ESP_LOGI(TAG, "Stopping application...");
    
    m_running = false;
    
    m_wake_word_service.stop();
    m_audio_pipeline.stop();
    m_ota_service.stop();
    m_uart_driver.stop();
    m_mqtt_service.stop();
    m_wifi_service.stop();
    m_display.stop();
    m_audio_state_machine.stop();
    m_state_machine.stop();
    m_power_manager.stop();
    m_watchdog.stop();
    
    WifiConfigService::getInstance().deinitialize();
    EventBus::getInstance().deinitialize();
    
    ESP_LOGI(TAG, "Application stopped");
}

void Application::setupEventSubscriptions() {
    // Subscribe to state changes
    EventBus::getInstance().subscribe(EventType::STATE_CHANGED, [this](const EventMessage& event) {
        handleStateChange(event);
    });
    
    // Subscribe to wake word events
    EventBus::getInstance().subscribe(EventType::WAKE_WORD_DETECTED, [this](const EventMessage& event) {
        handleWakeWord(event);
    });
    
    // Subscribe to OTA events
    EventBus::getInstance().subscribe(EventType::OTA_STARTED, [this](const EventMessage& event) {
        (void)event;
        ESP_LOGI(TAG, "OTA update started");
    });
    
    EventBus::getInstance().subscribe(EventType::OTA_PROGRESS, [this](const EventMessage& event) {
        ESP_LOGI(TAG, "OTA progress: %lu%% - %s",
                event.payload.ota_info.progress_percent,
                event.payload.ota_info.status);
    });
    
    // Subscribe to UART events
    EventBus::getInstance().subscribe(EventType::UART_DATA_RECEIVED, [this](const EventMessage& event) {
        ESP_LOGI(TAG, "UART data received: %zu bytes", event.payload.uart_data.data_len);
        // Handle UART commands here
    });
    
    // Subscribe to MQTT commands for OTA
    EventBus::getInstance().subscribe(EventType::MQTT_DATA_RECEIVED, [this](const EventMessage& event) {
        if (strstr(event.payload.mqtt_data.topic, "ota/trigger") != nullptr) {
            const char* url = event.payload.mqtt_data.data;
            ESP_LOGI(TAG, "OTA triggered via MQTT: %s", url);
            m_ota_service.startOTA(url);
        }
    });
    
    // Subscribe to WiFi events for display updates
    EventBus::getInstance().subscribe(EventType::WIFI_CONNECTED, [this](const EventMessage& event) {
        (void)event;
        DisplayUpdate update;
        update.type = DisplayUpdate::WIFI_STATUS;
        update.line1 = "WiFi Connected";
        m_display.updateDisplay(update);
    });
    
    EventBus::getInstance().subscribe(EventType::WIFI_GOT_IP, [this](const EventMessage& event) {
        DisplayUpdate update;
        update.type = DisplayUpdate::WIFI_STATUS;
        update.line1 = "WiFi: " + m_wifi_service.getIPAddress();
        m_display.updateDisplay(update);
    });
    
    // Subscribe to MQTT events for display updates
    EventBus::getInstance().subscribe(EventType::MQTT_CONNECTED, [this](const EventMessage& event) {
        (void)event;
        DisplayUpdate update;
        update.type = DisplayUpdate::MQTT_STATUS;
        update.line1 = "MQTT Connected";
        m_display.updateDisplay(update);
    });
}

void Application::handleStateChange(const EventMessage& event) {
    ESP_LOGI(TAG, "State changed: %s", event.payload.state_info.state_name);
    
    // Update display
    DisplayUpdate update;
    update.type = DisplayUpdate::APP_STATE;
    update.line1 = "State: " + std::string(event.payload.state_info.state_name);
    m_display.updateDisplay(update);
    
    // Handle MQTT connection when WiFi is connected
    if (m_state_machine.getState() == AppState::WIFI_CONNECTED) {
        if (!m_mqtt_service.isConnected()) {
            ESP_LOGI(TAG, "Attempting MQTT connection...");
            m_mqtt_service.connect();
        }
    }
}

void Application::handleWakeWord(const EventMessage& event) {
    (void)event;
    ESP_LOGI(TAG, "Wake word detected!");
    
    // Publish wake word event to MQTT
    if (m_mqtt_service.isConnected()) {
        const char* topic = "smart_home/wake_word";
        const char* data = "{\"event\":\"wake_word_detected\",\"timestamp\":1234567890}";
        m_mqtt_service.publish(topic, data, strlen(data));
    }
    
    // Update display
    DisplayUpdate update;
    update.type = DisplayUpdate::TEXT;
    update.line1 = "Wake Word";
    update.line2 = "Detected!";
    m_display.updateDisplay(update);
}

