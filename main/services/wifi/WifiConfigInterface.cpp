/**
 * @file WifiConfigInterface.cpp
 * @brief Unified WiFi Configuration and Provisioning Interface implementation
 */

#include "WifiConfigInterface.h"
#include "core/console/Console.h"

#include <cstring>
#include <iterator>

const char* WifiConfigInterface::TAG = "WifiConfigInterface";
const char* WifiConfigInterface::NVS_NAMESPACE = "wifi_config";
const char* WifiConfigInterface::KEY_SSID = "ssid";
const char* WifiConfigInterface::KEY_PASSWORD = "password";

WifiConfigInterface& WifiConfigInterface::getInstance() {
    static WifiConfigInterface instance;
    return instance;
}

bool WifiConfigInterface::initialize() {
    if (m_initialized) {
        return true;
    }

    ESP_LOGI(TAG, "Initializing WiFi Config Interface...");

    // Initialize mutex
    m_mutex = xSemaphoreCreateMutex();
    if (m_mutex == nullptr) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return false;
    }

    // Open NVS
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &m_nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        vSemaphoreDelete(m_mutex);
        m_mutex = nullptr;
        return false;
    }

    // Register console commands
    if (!registerConsoleCommands()) {
        ESP_LOGW(TAG, "Failed to register console commands");
    }

    m_initialized = true;
    ESP_LOGI(TAG, "WiFi Config Interface initialized");
    return true;
}

bool WifiConfigInterface::registerConsoleCommands() {
    // Local static: the registry holds the table by pointer, so it must have
    // static storage duration. Declaring it here rather than at file scope
    // keeps the handlers private and the table next to where it is used.
    static const console::Command COMMANDS[] = {
        {"wifi_set",
         "Set WiFi SSID and password\nUsage: wifi_set <ssid> <password>",
         "wifi_set <ssid> <password>",
         &WifiConfigInterface::consoleSetBoth},

        {"wifi_ssid",
         "Set WiFi SSID\nUsage: wifi_ssid <ssid>",
         "wifi_ssid <ssid>",
         &WifiConfigInterface::consoleSetSSID},

        {"wifi_password",
         "Set WiFi password\nUsage: wifi_password <password>",
         "wifi_password <password>",
         &WifiConfigInterface::consoleSetPassword},

        {"wifi_status",
         "Show WiFi configuration status",
         nullptr,
         &WifiConfigInterface::consoleStatus},

        {"wifi_clear",
         "Clear WiFi credentials",
         nullptr,
         &WifiConfigInterface::consoleClear},
    };

    return console::addFeature("wifi", COMMANDS, std::size(COMMANDS));
}

bool WifiConfigInterface::setSSID(const char* ssid) {
    if (!m_initialized || ssid == nullptr) {
        return false;
    }

    if (xSemaphoreTake(m_mutex, portMAX_DELAY) != pdTRUE) {
        return false;
    }

    esp_err_t err = nvs_set_str(m_nvs_handle, KEY_SSID, ssid);
    if (err == ESP_OK) {
        err = nvs_commit(m_nvs_handle);
    }

    xSemaphoreGive(m_mutex);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set SSID: %s", esp_err_to_name(err));
        return false;
    }

    ESP_LOGI(TAG, "SSID saved: %s", ssid);
    return true;
}

bool WifiConfigInterface::setPassword(const char* password) {
    if (!m_initialized || password == nullptr) {
        return false;
    }

    if (xSemaphoreTake(m_mutex, portMAX_DELAY) != pdTRUE) {
        return false;
    }

    esp_err_t err = nvs_set_str(m_nvs_handle, KEY_PASSWORD, password);
    if (err == ESP_OK) {
        err = nvs_commit(m_nvs_handle);
    }

    xSemaphoreGive(m_mutex);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set password: %s", esp_err_to_name(err));
        return false;
    }

    ESP_LOGI(TAG, "Password saved");
    return true;
}

bool WifiConfigInterface::getSSID(char* ssid, size_t max_len) {
    if (!m_initialized || ssid == nullptr || max_len == 0) {
        return false;
    }

    if (xSemaphoreTake(m_mutex, portMAX_DELAY) != pdTRUE) {
        return false;
    }

    size_t required_size = max_len;
    esp_err_t err = nvs_get_str(m_nvs_handle, KEY_SSID, ssid, &required_size);

    xSemaphoreGive(m_mutex);

    if (err == ESP_OK) {
        return true;
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "SSID not found in NVS");
    } else {
        ESP_LOGE(TAG, "Failed to get SSID: %s", esp_err_to_name(err));
    }

    return false;
}

bool WifiConfigInterface::getPassword(char* password, size_t max_len) {
    if (!m_initialized || password == nullptr || max_len == 0) {
        return false;
    }

    if (xSemaphoreTake(m_mutex, portMAX_DELAY) != pdTRUE) {
        return false;
    }

    size_t required_size = max_len;
    esp_err_t err = nvs_get_str(m_nvs_handle, KEY_PASSWORD, password, &required_size);

    xSemaphoreGive(m_mutex);

    if (err == ESP_OK) {
        return true;
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Password not found in NVS");
    } else {
        ESP_LOGE(TAG, "Failed to get password: %s", esp_err_to_name(err));
    }

    return false;
}

bool WifiConfigInterface::setCredentials(const char* ssid, const char* password) {
    if (!ssid || !password) {
        ESP_LOGE(TAG, "Invalid credentials");
        return false;
    }

    if (!setSSID(ssid) || !setPassword(password)) {
        ESP_LOGE(TAG, "Failed to set credentials");
        return false;
    }

    ESP_LOGI(TAG, "WiFi credentials configured: %s", ssid);
    return true;
}

bool WifiConfigInterface::hasCredentials() {
    WifiConfigInfo_st wifi_cfg;
    return getSSID(wifi_cfg.ssid, sizeof(wifi_cfg.ssid)) && 
           getPassword(wifi_cfg.password, sizeof(wifi_cfg.password));
}

bool WifiConfigInterface::hasCredentials(WifiConfigInfo_st* wifi_cfg) {
    if (wifi_cfg == nullptr) {
        return false;
    }
    return getSSID(wifi_cfg->ssid, sizeof(wifi_cfg->ssid)) && 
           getPassword(wifi_cfg->password, sizeof(wifi_cfg->password));
}

bool WifiConfigInterface::clearCredentials() {
    if (!setSSID("") || !setPassword("")) {
        ESP_LOGE(TAG, "Failed to clear credentials");
        return false;
    }

    ESP_LOGI(TAG, "WiFi credentials cleared");
    return true;
}

void WifiConfigInterface::printStatus() {
    char ssid[33] = {0};

    if (getSSID(ssid, sizeof(ssid))) {
        printf("WiFi SSID: %s\n", ssid);
        printf("Password: [CONFIGURED]\n");
        printf("Status: Ready to connect\n");
    } else {
        printf("WiFi: Not configured\n");
        printf("Status: Need credentials\n");
        printf("\nTo configure WiFi, use:\n");
        printf("  wifi_set <ssid> <password>\n");
        printf("Example:\n");
        printf("  wifi_set MyNetwork MyPassword123\n");
    }
}

bool WifiConfigInterface::setDefaultCredentials(const char* ssid, const char* password) {
    if (hasCredentials()) {
        ESP_LOGI(TAG, "Credentials already configured, skipping defaults");
        return false;
    }

    ESP_LOGI(TAG, "No credentials found, setting defaults...");
    return setCredentials(ssid, password);
}

// Console command handlers
int WifiConfigInterface::consoleSetSSID(int argc, char **argv) {
    if (!console::requireArgs(argc, 2, "wifi_ssid <ssid>")) {
        return 1;
    }

    if (WifiConfigInterface::getInstance().setSSID(argv[1])) {
        printf("SSID set to: %s\n", argv[1]);
        return 0;
    } else {
        printf("Failed to set SSID\n");
        return 1;
    }
}

int WifiConfigInterface::consoleSetPassword(int argc, char **argv) {
    if (!console::requireArgs(argc, 2, "wifi_password <password>")) {
        return 1;
    }

    if (WifiConfigInterface::getInstance().setPassword(argv[1])) {
        printf("Password updated\n");
        return 0;
    } else {
        printf("Failed to set password\n");
        return 1;
    }
}

int WifiConfigInterface::consoleSetBoth(int argc, char **argv) {
    if (!console::requireArgs(argc, 3, "wifi_set <ssid> <password>")) {
        return 1;
    }

    if (WifiConfigInterface::getInstance().setCredentials(argv[1], argv[2])) {
        printf("WiFi configured successfully\n");
        printf("SSID: %s\n", argv[1]);
        printf("Password: [HIDDEN]\n");
        printf("Restart to apply changes\n");
        return 0;
    } else {
        printf("Failed to configure WiFi\n");
        return 1;
    }
}

int WifiConfigInterface::consoleStatus(int argc, char **argv) {
    WifiConfigInterface::getInstance().printStatus();
    return 0;
}

int WifiConfigInterface::consoleClear(int argc, char **argv) {
    if (WifiConfigInterface::getInstance().clearCredentials()) {
        printf("WiFi credentials cleared\n");
        return 0;
    } else {
        printf("Failed to clear credentials\n");
        return 1;
    }
}

void WifiConfigInterface::deinitialize() {
    if (m_initialized) {
        nvs_close(m_nvs_handle);
        m_initialized = false;
    }

    if (m_mutex != nullptr) {
        vSemaphoreDelete(m_mutex);
        m_mutex = nullptr;
    }

    ESP_LOGI(TAG, "WiFi Config Interface deinitialized");
}
