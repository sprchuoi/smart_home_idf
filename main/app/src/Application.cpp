/**
 * @file Application.cpp
 * @brief Main Application implementation
 */

#include "cfg/Application_cfg.hpp"

#include <string>
#include <cstring>
#include "nvs_flash.h"
#include "esp_console.h"
#include "driver/uart.h"
#include "linenoise/linenoise.h"


namespace {
// How often run() logs its liveness line.
constexpr uint32_t APP_STATUS_INTERVAL_MS = 2000;
}  // namespace

const char* Application::TAG = "Application";

Application::Application()
    : m_initialized(false)
    , m_running(false) 
    { // Initialize pointer to nullptr
}

Application::~Application() {
    stop();
}

bool Application::initialize() {
    if (m_initialized) {
        return true;
    }
    
    ESP_LOGI(TAG, "Initializing Smart Home Application...");
    
    // Initialize ESP Console
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = "esp32>";
    repl_config.max_cmdline_length = 256;
    
    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_config, &repl_config, &repl));
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
    
    ESP_LOGI(TAG, "Console initialized");
    
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
    
    // Initialize WiFi Config Interface (combines config and provisioning)
    if (!WifiConfigInterface::getInstance().initialize()) {
        ESP_LOGE(TAG, "Failed to initialize WifiConfigInterface");
        return false;
    }

    // --- WiFi ---------------------------------------------------------------
    // Credentials come from NVS, provisioned at runtime over the console:
    //     esp32> wifi_set <ssid> <password>
    // A device with no credentials boots to a usable console instead of
    // retrying forever.
    if (!WifiConfigInterface::getInstance().hasCredentials(&g_wifi_cfg)) {
        ESP_LOGW(TAG, "No WiFi credentials configured.");
        ESP_LOGW(TAG, "Provision over the console, then reboot:");
        ESP_LOGW(TAG, "  wifi_set <ssid> <password>");
    } else if (!m_wifi_service.initialize(&g_wifi_cfg)) {
        ESP_LOGE(TAG, "Failed to initialize WifiService");
        return false;
    } else if (!m_wifi_service.connect()) {
        // WifiService::initialize() only configures the driver -- esp_wifi_start()
        // lives in connect(). That call was missing entirely, so the radio never
        // came up, no WIFI_CONNECTED event was ever published, and everything
        // downstream (state machine, MQTT) silently never ran.
        ESP_LOGE(TAG, "Failed to start WiFi");
        return false;
    } else {
        ESP_LOGI(TAG, "WiFi started, awaiting connection");
    }
     
    // --- Not yet enabled ----------------------------------------------------
    // These services are compiled but deliberately left off until a later phase,
    // listed here so the gap is visible rather than looking like working config:
    //
    //   AppStateMachine    event graph is unreachable -- WIFI_STARTED and
    //                      WIFI_GOT_IP are never published by anything, so it
    //                      would sit in INIT forever.
    //   MqttService        needs broker identity from NVS, not a hardcoded URI.
    //   WatchdogSupervisor initialize() never calls esp_task_wdt_init(), so the
    //                      configured 30 s timeout is fiction today.
    //   OTAService         written against esp_https_ota() without ever holding
    //                      a handle, so its success path is dead code.
    //   OledDisplay        render path is a stub -- no framebuffer, no font.
    //   PowerManager       MODEM_SLEEP is unimplemented and LIGHT_SLEEP can sleep
    //                      indefinitely on a zero-length timer.
    //
    // UartDriver is off by design: the ESP console already owns UART0.
    // AudioPipeline, AudioStateMachine and WakeWordService were removed -- they
    // were scaffolding with no working wake-word model behind them, and
    // AudioPipeline does not compile for the ESP32-S3 at all.
    
    // Note: Watchdog tasks will be registered in start() after all services are fully running

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

    // UartDriver is deliberately neither started nor stopped here.
    // It defaults to UART0, which the esp_console REPL already owns, so start()
    // would fail anyway -- and its stop() is unguarded: it calls
    // uart_driver_delete(UART_NUM_0) against the console's driver and then
    // double-frees the event queue that uart_driver_delete already released.
    // Point it at UART1 with explicit ESP32-S3 pins before enabling it.
    //
    // Watchdog registration is likewise skipped: WatchdogSupervisor::initialize()
    // never calls esp_task_wdt_init(), so registerTask() early-returns and
    // listing tasks here would imply supervision that is not happening.

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

    // Status heartbeat. There is no display yet -- OledDisplay's render path is
    // a stub and it is not initialized -- so this exists to prove the scheduler
    // is alive and to surface link state at a glance over serial.
    while (m_running) {
        ESP_LOGI(TAG, "alive | wifi:%s | ip:%s",
                 m_wifi_service.isConnected() ? "up" : "down",
                 m_wifi_service.getIPAddress().c_str());
        vTaskDelay(pdMS_TO_TICKS(APP_STATUS_INTERVAL_MS));
    }
}

void Application::stop() {
    if (!m_running) {
        return;
    }
    
    ESP_LOGI(TAG, "Stopping application...");
    
    m_running = false;
    
    // m_uart_driver.stop() is intentionally absent -- see start() for why.
    m_ota_service.stop();
    m_mqtt_service.stop();
    m_wifi_service.stop();
    m_display.stop();
    m_state_machine.stop();
    m_power_manager.stop();
    m_watchdog.stop();
    
    WifiConfigInterface::getInstance().deinitialize();
    EventBus::getInstance().deinitialize();
    
    ESP_LOGI(TAG, "Application stopped");
}

// NOTE: this is currently unreachable -- nothing publishes STATE_CHANGED and
// AppStateMachine is never initialized. It is kept because it is the intended
// hook for bringing MQTT up once WiFi connects. Do not assume it runs.
void Application::handleStateChange(const EventMessage& event) {
    ESP_LOGI(TAG, "State changed: %s", event.payload.state_info.text);

    // Handle MQTT connection when WiFi is connected
    if (m_state_machine.getState() == AppState::WIFI_CONNECTED) {
        if (!m_mqtt_service.isConnected()) {
            ESP_LOGI(TAG, "Attempting MQTT connection...");
            m_mqtt_service.connect();
        }
    }
}

