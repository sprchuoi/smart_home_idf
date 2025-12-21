/**
 * @file WifiProvisioning.cpp
 * @brief WiFi provisioning service implementation
 */

#include "WifiProvisioning.h"
#include <cstring>

const char* WifiProvisioning::TAG = "WifiProvisioning";

WifiProvisioning& WifiProvisioning::getInstance() {
    static WifiProvisioning instance;
    return instance;
}

bool WifiProvisioning::initialize() {
    if (m_initialized) {
        return true;
    }
    
    ESP_LOGI(TAG, "Initializing WiFi Provisioning...");
    
    // Register console commands
    if (!registerConsoleCommands()) {
        ESP_LOGW(TAG, "Failed to register console commands");
    }
    
    m_initialized = true;
    ESP_LOGI(TAG, "WiFi Provisioning initialized");
    return true;
}

bool WifiProvisioning::registerConsoleCommands() {
    esp_console_cmd_t cmd;
    
    // wifi_set command - set both SSID and password
    cmd = {
        .command = "wifi_set",
        .help = "Set WiFi SSID and password\n"
                "Usage: wifi_set <ssid> <password>",
        .hint = NULL,
        .func = &WifiProvisioning::consoleSetBoth,
        .argtable = NULL
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
    
    // wifi_ssid command - set SSID only
    cmd = {
        .command = "wifi_ssid",
        .help = "Set WiFi SSID\n"
                "Usage: wifi_ssid <ssid>",
        .hint = NULL,
        .func = &WifiProvisioning::consoleSetSSID,
        .argtable = NULL
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
    
    // wifi_password command - set password only
    cmd = {
        .command = "wifi_password",
        .help = "Set WiFi password\n"
                "Usage: wifi_password <password>",
        .hint = NULL,
        .func = &WifiProvisioning::consoleSetPassword,
        .argtable = NULL
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
    
    // wifi_status command
    cmd = {
        .command = "wifi_status",
        .help = "Show WiFi configuration status",
        .hint = NULL,
        .func = &WifiProvisioning::consoleStatus,
        .argtable = NULL
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
    
    // wifi_clear command
    cmd = {
        .command = "wifi_clear",
        .help = "Clear WiFi credentials",
        .hint = NULL,
        .func = &WifiProvisioning::consoleClear,
        .argtable = NULL
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
    
    ESP_LOGI(TAG, "Console commands registered");
    return true;
}

bool WifiProvisioning::setCredentials(const char* ssid, const char* password) {
    if (!ssid || !password) {
        ESP_LOGE(TAG, "Invalid credentials");
        return false;
    }
    
    if (!WifiConfigService::getInstance().setSSID(ssid)) {
        ESP_LOGE(TAG, "Failed to set SSID");
        return false;
    }
    
    if (!WifiConfigService::getInstance().setPassword(password)) {
        ESP_LOGE(TAG, "Failed to set password");
        return false;
    }
    
    ESP_LOGI(TAG, "WiFi credentials configured: %s", ssid);
    return true;
}

bool WifiProvisioning::hasCredentials() {
    WifiConfigInfo_st wifi_cfg;
    return WifiConfigService::getInstance().hasCredentials(&wifi_cfg);
}

bool WifiProvisioning::hasCredentials(WifiConfigInfo_st* wifi_cfg) {
    return WifiConfigService::getInstance().hasCredentials(wifi_cfg);
}

bool WifiProvisioning::clearCredentials() {
    // Set empty strings to clear
    if (!WifiConfigService::getInstance().setSSID("")) {
        return false;
    }
    if (!WifiConfigService::getInstance().setPassword("")) {
        return false;
    }
    
    ESP_LOGI(TAG, "WiFi credentials cleared");
    return true;
}

void WifiProvisioning::printStatus() {
    char ssid[33] = {0};
    
    if (WifiConfigService::getInstance().getSSID(ssid, sizeof(ssid))) {
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

bool WifiProvisioning::setDefaultCredentials(const char* ssid, const char* password) {
    if (hasCredentials()) {
        ESP_LOGI(TAG, "Credentials already configured, skipping defaults");
        return false;
    }
    
    ESP_LOGI(TAG, "No credentials found, setting defaults...");
    return setCredentials(ssid, password);
}

// Console command handlers
int WifiProvisioning::consoleSetSSID(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: wifi_ssid <ssid>\n");
        return 1;
    }
    
    const char* ssid = argv[1];
    
    if (WifiConfigService::getInstance().setSSID(ssid)) {
        printf("SSID set to: %s\n", ssid);
        return 0;
    } else {
        printf("Failed to set SSID\n");
        return 1;
    }
}

int WifiProvisioning::consoleSetPassword(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: wifi_password <password>\n");
        return 1;
    }
    
    const char* password = argv[1];
    
    if (WifiConfigService::getInstance().setPassword(password)) {
        printf("Password updated\n");
        return 0;
    } else {
        printf("Failed to set password\n");
        return 1;
    }
}

int WifiProvisioning::consoleSetBoth(int argc, char **argv) {
    if (argc < 3) {
        printf("Usage: wifi_set <ssid> <password>\n");
        return 1;
    }
    
    const char* ssid = argv[1];
    const char* password = argv[2];
    
    if (WifiProvisioning::getInstance().setCredentials(ssid, password)) {
        printf("WiFi configured successfully\n");
        printf("SSID: %s\n", ssid);
        printf("Password: [HIDDEN]\n");
        printf("\nRestart to apply changes\n");
        return 0;
    } else {
        printf("Failed to configure WiFi\n");
        return 1;
    }
}

int WifiProvisioning::consoleStatus(int argc, char **argv) {
    WifiProvisioning::getInstance().printStatus();
    return 0;
}

int WifiProvisioning::consoleClear(int argc, char **argv) {
    if (WifiProvisioning::getInstance().clearCredentials()) {
        printf("WiFi credentials cleared\n");
        return 0;
    } else {
        printf("Failed to clear credentials\n");
        return 1;
    }
}
