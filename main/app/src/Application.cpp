/**
 * @file Application.cpp
 * @brief Main Application implementation
 */

#include "app/src/Application.h"

#include <cstdio>
#include <cstring>
#include <string>

#include "cJSON.h"
#include "nvs_flash.h"
#include "esp_console.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "linenoise/linenoise.h"

namespace {
// How often run() publishes link diagnostics and logs a heartbeat.
constexpr uint32_t TELEMETRY_INTERVAL_MS = 30000;

// Refresh the retained status document every N telemetry cycles (so every
// 30 s * 10 = 5 minutes). See publishTelemetry() for why this is needed at all.
constexpr uint32_t STATUS_EVERY_N_TICKS = 10;
}  // namespace

const char* Application::TAG = "Application";

Application::Application() = default;

Application::~Application() {
    stop();
}

bool Application::initialize() {
    if (m_initialized) {
        return true;
    }

    ESP_LOGI(TAG, "Initializing Smart Home Application...");

    // --- Console -------------------------------------------------------------
    esp_console_repl_t* repl = nullptr;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = "esp32>";
    repl_config.max_cmdline_length = 256;

    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_config, &repl_config, &repl));
    ESP_ERROR_CHECK(esp_console_start_repl(repl));

    // --- NVS -----------------------------------------------------------------
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition truncated; erasing and retrying");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // --- Config stores -------------------------------------------------------
    if (!WifiConfigInterface::getInstance().initialize()) {
        ESP_LOGE(TAG, "Failed to initialize WifiConfigInterface");
        return false;
    }
    if (!MqttConfigInterface::getInstance().initialize()) {
        ESP_LOGE(TAG, "Failed to initialize MqttConfigInterface");
        return false;
    }

    // --- WiFi ----------------------------------------------------------------
    // Credentials live in NVS, provisioned at runtime over the console. A node
    // with none boots to a usable console instead of retrying forever.
    if (!WifiConfigInterface::getInstance().hasCredentials(&m_wifi_cfg)) {
        ESP_LOGW(TAG, "No WiFi credentials. Provision, then reboot:");
        ESP_LOGW(TAG, "  wifi_set <ssid> <password>");
    } else if (!m_wifi_service.initialize(&m_wifi_cfg)) {
        ESP_LOGE(TAG, "Failed to initialize WifiService");
        return false;
    } else {
        m_wifi_service.setEventCallback(
            [this](const WifiQueueEvent& e) { onWifiEvent(e); });
        m_state_machine.setState(AppState::WIFI_CONNECTING);
        if (!m_wifi_service.connect()) {
            // WifiService::initialize() only configures the driver --
            // esp_wifi_start() lives in connect().
            ESP_LOGE(TAG, "Failed to start WiFi");
            return false;
        }
    }

    // --- MQTT ----------------------------------------------------------------
    if (!MqttConfigInterface::getInstance().hasConfig(&m_mqtt_cfg)) {
        ESP_LOGW(TAG, "MQTT not provisioned. Provision, then reboot:");
        ESP_LOGW(TAG, "  mqtt_set <host> [port]");
        ESP_LOGW(TAG, "  mqtt_device <device_id> <name> [room]");
    } else if (!m_mqtt_service.initialize(&m_mqtt_cfg)) {
        ESP_LOGE(TAG, "Failed to initialize MqttService");
        return false;
    } else {
        m_mqtt_service.setCommandCallback(
            [this](const char* t, size_t tl, const char* d, size_t dl) {
                onMqttCommand(t, tl, d, dl);
            });
        m_mqtt_service.setConnectionCallback(
            [this](bool connected) { onMqttConnection(connected); });
        // Not started here -- it waits for an IP address, in onWifiEvent().
    }

    // --- OTA -----------------------------------------------------------------
    if (!m_ota_service.initialize()) {
        ESP_LOGW(TAG, "Failed to initialize OTAService (continuing)");
    } else {
        m_ota_service.setProgressCallback([this](int percent) {
            if (!m_mqtt_service.isConnected()) {
                return;
            }
            char status[48];
            snprintf(status, sizeof(status), "{\"ota\":%d}", percent);
            m_mqtt_service.publishStatus(status);
        });
    }

    // UartDriver is deliberately not started: it defaults to UART0, which the
    // console REPL above already owns, and its stop() is unguarded (it would
    // delete the console's driver and double-free its queue). Point it at
    // UART1 with explicit ESP32-S3 pins before enabling it.

    m_initialized = true;
    ESP_LOGI(TAG, "Initialized. state=%s", m_state_machine.getStateString());
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
    m_running = true;
    ESP_LOGI(TAG, "Application started");
    return true;
}

void Application::onWifiEvent(const WifiQueueEvent& event) {
    switch (event.type) {
        case WifiEventType::STA_START:
            ESP_LOGI(TAG, "WiFi station started");
            m_state_machine.setState(AppState::WIFI_CONNECTING);
            break;

        case WifiEventType::GOT_IP: {
            char ip[16] = {};
            esp_ip4addr_ntoa(reinterpret_cast<const esp_ip4_addr_t*>(&event.ip_addr), ip, sizeof(ip));
            ESP_LOGI(TAG, "Got IP %s", ip);
            m_state_machine.setState(AppState::MQTT_CONNECTING);
            m_mqtt_service.start();
            break;
        }

        case WifiEventType::DISCONNECTED:
            ESP_LOGW(TAG, "WiFi disconnected (reason %u)", (unsigned)event.reason);
            m_state_machine.setState(AppState::WIFI_CONNECTING);
            // Drop the broker connection now so the Last Will fires promptly
            // instead of waiting out the 45 s keepalive.
            m_mqtt_service.notifyWifiDown();
            break;
    }
}

void Application::onMqttConnection(bool connected) {
    // Runs in the esp-mqtt task. setState() is an atomic exchange plus a log
    // line, so it is safe here.
    if (connected) {
        m_state_machine.setState(AppState::RUNNING);
    } else if (m_state_machine.getState() == AppState::RUNNING) {
        // Back to CONNECTING rather than ERROR: a broker restart is expected
        // and the reconnect loop will handle it.
        m_state_machine.setState(AppState::MQTT_CONNECTING);
    }
}

void Application::onMqttCommand(const char* topic, size_t topic_len,
                                const char* data, size_t data_len) {
    // The topic is always <prefix>/command; the verb lives in the body, which
    // is how Smart_Server publishes it.
    (void)topic;
    (void)topic_len;

    // Copy out of esp-mqtt's buffer -- it is not NUL-terminated and is only
    // valid for the duration of this callback.
    const std::string payload(data, data_len);

    cJSON* root = cJSON_ParseWithLength(payload.c_str(), payload.size());
    if (root == nullptr) {
        ESP_LOGW(TAG, "Command is not valid JSON: %s", payload.c_str());
        return;
    }

    const cJSON* cmd = cJSON_GetObjectItemCaseSensitive(root, "command");
    if (!cJSON_IsString(cmd) || cmd->valuestring == nullptr) {
        ESP_LOGW(TAG, "Command has no 'command' field: %s", payload.c_str());
        cJSON_Delete(root);
        return;
    }

    const std::string verb = cmd->valuestring;
    ESP_LOGI(TAG, "Command '%s'", verb.c_str());

    bool ok = true;

    if (verb == "reboot") {
        // Acknowledge before restarting, or the server never learns it landed.
        char ack[96];
        snprintf(ack, sizeof(ack),
                 "{\"command\":\"reboot\",\"status\":\"ok\",\"device_id\":\"%s\"}",
                 m_mqtt_cfg.device_id);
        m_mqtt_service.publishResponse(ack);
        cJSON_Delete(root);

        ESP_LOGW(TAG, "Rebooting at server request");
        vTaskDelay(pdMS_TO_TICKS(200));  // let the ack reach the broker
        esp_restart();
        return;
    }

    if (verb == "get_status") {
        m_mqtt_service.publishDeviceStatus();
    } else if (verb == "ota") {
        // Only sets a flag; the OTA task performs the transfer, so a slow
        // download never blocks the MQTT client.
        const cJSON* url = cJSON_GetObjectItemCaseSensitive(root, "url");
        const char* target =
            (cJSON_IsString(url) && url->valuestring[0] != '\0') ? url->valuestring
                                                                : m_mqtt_cfg.ota_url;
        if (target == nullptr || target[0] == '\0') {
            ESP_LOGW(TAG, "OTA command carries no URL and none is configured");
            ok = false;
        } else {
            ok = m_ota_service.requestUpdate(target) || m_ota_service.isOTAInProgress();
        }
    } else {
        ESP_LOGW(TAG, "Unknown command '%s'", verb.c_str());
        ok = false;
    }

    // Built with cJSON rather than snprintf: the verb is a string we did not
    // author, and echoing it unescaped could emit malformed JSON.
    cJSON* ack = cJSON_CreateObject();
    cJSON_AddStringToObject(ack, "command", verb.c_str());
    cJSON_AddStringToObject(ack, "status", ok ? "ok" : "error");
    cJSON_AddStringToObject(ack, "device_id", m_mqtt_cfg.device_id);
    char* ack_text = cJSON_PrintUnformatted(ack);
    if (ack_text != nullptr) {
        m_mqtt_service.publishResponse(ack_text);
        cJSON_free(ack_text);
    }
    cJSON_Delete(ack);
    cJSON_Delete(root);
}

void Application::publishTelemetry() {
    if (!m_mqtt_service.isConnected()) {
        return;
    }

    // These three need no sensor hardware, so they exercise the whole path --
    // broker, storage, database -- before Phase 3 wires up anything real.
    wifi_ap_record_t ap = {};
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
        m_mqtt_service.publishSensor("rssi", (float)ap.rssi, "dBm");
    }

    m_mqtt_service.publishSensor(
        "heap", (float)heap_caps_get_free_size(MALLOC_CAP_INTERNAL), "B");

    m_mqtt_service.publishSensor(
        "uptime", (float)(esp_timer_get_time() / 1000000), "s");

    // Refresh the status document periodically, not only on connect.
    //
    // Smart_Server's bridge stamps Device.last_seen when it handles a *status*
    // message; sensor messages do not touch it. Publishing status only on
    // connect would therefore leave last_seen frozen at boot, and the server
    // would consider a perfectly healthy node stale within one keepalive.
    //
    // Every STATUS_EVERY_N_TICKS cycles rather than every cycle: this is a
    // retained QoS 1 publish, and hammering it would be 2880 retained writes
    // a day for a value that barely changes.
    if (++m_telemetry_ticks >= STATUS_EVERY_N_TICKS) {
        m_telemetry_ticks = 0;
        m_mqtt_service.publishDeviceStatus();
    }
}

void Application::run() {
    if (!m_running && !start()) {
        ESP_LOGE(TAG, "Failed to start application");
        return;
    }

    ESP_LOGI(TAG, "Application running...");

    // The RUNNING transition comes from onMqttConnection(), not from here:
    // run() is reached before the broker connection completes, so a check at
    // this point would always be false and the state would never advance past
    // MQTT_CONNECTING.

    while (m_running) {
        ESP_LOGI(TAG, "alive | state:%s | wifi:%s | mqtt:%s",
                 m_state_machine.getStateString(),
                 m_wifi_service.isConnected() ? "up" : "down",
                 m_mqtt_service.isConnected() ? "up" : "down");
        publishTelemetry();
        vTaskDelay(pdMS_TO_TICKS(TELEMETRY_INTERVAL_MS));
    }
}

void Application::stop() {
    if (!m_running) {
        return;
    }
    ESP_LOGI(TAG, "Stopping application...");
    m_running = false;

    // m_uart_driver.stop() is intentionally absent -- see initialize().
    m_ota_service.stop();
    m_mqtt_service.stop();
    m_wifi_service.stop();

    MqttConfigInterface::getInstance().deinitialize();
    WifiConfigInterface::getInstance().deinitialize();

    ESP_LOGI(TAG, "Application stopped");
}
