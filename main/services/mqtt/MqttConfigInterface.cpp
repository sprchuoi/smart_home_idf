/**
 * @file MqttConfigInterface.cpp
 * @brief MQTT configuration implementation
 */

#include "MqttConfigInterface.h"

#include "esp_mac.h"
#include <cstring>
#include <cstdio>

const char* MqttConfigInterface::TAG = "MqttConfig";
const char* MqttConfigInterface::NVS_NAMESPACE = "mqtt_config";
const char* MqttConfigInterface::KEY_HOST    = "host";
const char* MqttConfigInterface::KEY_PORT    = "port";
const char* MqttConfigInterface::KEY_USER    = "user";
const char* MqttConfigInterface::KEY_PASS    = "pass";
const char* MqttConfigInterface::KEY_DEVID   = "devid";
const char* MqttConfigInterface::KEY_NAME    = "name";
const char* MqttConfigInterface::KEY_ROOM    = "room";
const char* MqttConfigInterface::KEY_OTA_URL = "ota_url";

namespace {

/**
 * Read a string key, tolerating absence. nvs_get_str fails if the stored value
 * would not fit in `cap`, which is the behaviour we want -- a truncated broker
 * hostname or password is worse than none.
 */
void readStr(nvs_handle_t handle, const char* key, char* out, size_t cap) {
    size_t len = cap;
    if (nvs_get_str(handle, key, out, &len) != ESP_OK) {
        out[0] = '\0';
    }
}

}  // namespace

MqttConfigInterface& MqttConfigInterface::getInstance() {
    static MqttConfigInterface instance;
    return instance;
}

bool MqttConfigInterface::initialize() {
    if (m_initialized) {
        return true;
    }

    m_mutex = xSemaphoreCreateMutex();
    if (m_mutex == nullptr) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return false;
    }

    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &m_nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        vSemaphoreDelete(m_mutex);
        m_mutex = nullptr;
        return false;
    }

    if (!registerConsoleCommands()) {
        ESP_LOGW(TAG, "Failed to register console commands");
    }

    m_initialized = true;
    ESP_LOGI(TAG, "MQTT Config Interface initialized");
    return true;
}

void MqttConfigInterface::deinitialize() {
    if (m_nvs_handle != 0) {
        nvs_close(m_nvs_handle);
        m_nvs_handle = 0;
    }
    if (m_mutex != nullptr) {
        vSemaphoreDelete(m_mutex);
        m_mutex = nullptr;
    }
    m_initialized = false;
}

void MqttConfigInterface::load(MqttConfigInfo_st* cfg) {
    std::memset(cfg, 0, sizeof(*cfg));
    cfg->port = 1883;

    readStr(m_nvs_handle, KEY_HOST, cfg->host, sizeof(cfg->host));

    uint16_t port = 0;
    if (nvs_get_u16(m_nvs_handle, KEY_PORT, &port) == ESP_OK && port != 0) {
        cfg->port = port;
    }

    readStr(m_nvs_handle, KEY_USER, cfg->username, sizeof(cfg->username));
    readStr(m_nvs_handle, KEY_PASS, cfg->password, sizeof(cfg->password));
    readStr(m_nvs_handle, KEY_DEVID, cfg->device_id, sizeof(cfg->device_id));
    readStr(m_nvs_handle, KEY_NAME, cfg->name, sizeof(cfg->name));
    readStr(m_nvs_handle, KEY_ROOM, cfg->room, sizeof(cfg->room));
    readStr(m_nvs_handle, KEY_OTA_URL, cfg->ota_url, sizeof(cfg->ota_url));

    // Derive a stable identity from the factory MAC when none was set, and
    // persist it. Deriving it fresh on every boot would work, but persisting
    // means a future change to the derivation cannot silently orphan every
    // entity that Home Assistant already knows about.
    if (cfg->device_id[0] == '\0') {
        uint8_t mac[6] = {};
        if (esp_read_mac(mac, ESP_MAC_WIFI_STA) == ESP_OK) {
            snprintf(cfg->device_id, sizeof(cfg->device_id),
                     "shnode-%02x%02x%02x", mac[3], mac[4], mac[5]);
            nvs_set_str(m_nvs_handle, KEY_DEVID, cfg->device_id);
            nvs_commit(m_nvs_handle);
            ESP_LOGI(TAG, "Derived device id from MAC: %s", cfg->device_id);
        }
    }

    if (cfg->name[0] == '\0') {
        // Bounded precision: device_id may be up to MQTT_DEVID_MAX_LEN (24)
        // chars, and "Smart Home " + 24 would overflow the 33-byte name field,
        // which the compiler rightly rejects as a possible truncation.
        snprintf(cfg->name, sizeof(cfg->name), "Smart Home %.20s", cfg->device_id);
    }
}

bool MqttConfigInterface::hasConfig(MqttConfigInfo_st* cfg) {
    if (!m_initialized || cfg == nullptr) {
        return false;
    }
    if (xSemaphoreTake(m_mutex, portMAX_DELAY) != pdTRUE) {
        return false;
    }
    load(cfg);
    xSemaphoreGive(m_mutex);
    return cfg->host[0] != '\0';
}

bool MqttConfigInterface::isValidDeviceId(const char* id) {
    if (id == nullptr || id[0] == '\0') {
        return false;
    }
    for (const char* p = id; *p != '\0'; ++p) {
        const bool ok = (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') ||
                        *p == '-' || *p == '_';
        if (!ok) {
            return false;
        }
    }
    return std::strlen(id) <= MQTT_DEVID_MAX_LEN;
}

bool MqttConfigInterface::setHost(const char* host, uint16_t port) {
    if (!m_initialized || host == nullptr || host[0] == '\0') {
        return false;
    }
    if (xSemaphoreTake(m_mutex, portMAX_DELAY) != pdTRUE) {
        return false;
    }

    esp_err_t err = nvs_set_str(m_nvs_handle, KEY_HOST, host);
    if (err == ESP_OK) {
        err = nvs_set_u16(m_nvs_handle, KEY_PORT, port == 0 ? 1883 : port);
    }
    if (err == ESP_OK) {
        err = nvs_commit(m_nvs_handle);
    }

    xSemaphoreGive(m_mutex);
    return err == ESP_OK;
}

bool MqttConfigInterface::setCredentials(const char* user, const char* password) {
    if (!m_initialized) {
        return false;
    }
    if (xSemaphoreTake(m_mutex, portMAX_DELAY) != pdTRUE) {
        return false;
    }

    esp_err_t err = nvs_set_str(m_nvs_handle, KEY_USER, user != nullptr ? user : "");
    if (err == ESP_OK) {
        err = nvs_set_str(m_nvs_handle, KEY_PASS, password != nullptr ? password : "");
    }
    if (err == ESP_OK) {
        err = nvs_commit(m_nvs_handle);
    }

    xSemaphoreGive(m_mutex);
    return err == ESP_OK;
}

bool MqttConfigInterface::setDevice(const char* device_id, const char* name,
                                    const char* room) {
    if (!m_initialized || !isValidDeviceId(device_id)) {
        return false;
    }
    if (xSemaphoreTake(m_mutex, portMAX_DELAY) != pdTRUE) {
        return false;
    }

    esp_err_t err = nvs_set_str(m_nvs_handle, KEY_DEVID, device_id);
    if (err == ESP_OK) {
        err = nvs_set_str(m_nvs_handle, KEY_NAME, name != nullptr ? name : "");
    }
    if (err == ESP_OK) {
        err = nvs_set_str(m_nvs_handle, KEY_ROOM, room != nullptr ? room : "");
    }
    if (err == ESP_OK) {
        err = nvs_commit(m_nvs_handle);
    }

    xSemaphoreGive(m_mutex);
    return err == ESP_OK;
}

bool MqttConfigInterface::setOtaUrl(const char* url) {
    if (!m_initialized) {
        return false;
    }
    if (xSemaphoreTake(m_mutex, portMAX_DELAY) != pdTRUE) {
        return false;
    }

    esp_err_t err = nvs_set_str(m_nvs_handle, KEY_OTA_URL, url != nullptr ? url : "");
    if (err == ESP_OK) {
        err = nvs_commit(m_nvs_handle);
    }

    xSemaphoreGive(m_mutex);
    return err == ESP_OK;
}

bool MqttConfigInterface::clear() {
    if (!m_initialized) {
        return false;
    }
    if (xSemaphoreTake(m_mutex, portMAX_DELAY) != pdTRUE) {
        return false;
    }

    // Erase the keys rather than writing empty strings. Writing "" leaves the
    // key present, so hasConfig() would keep reporting the node as provisioned
    // -- a bug the WiFi equivalent actually has.
    esp_err_t err = nvs_erase_all(m_nvs_handle);
    if (err == ESP_OK) {
        err = nvs_commit(m_nvs_handle);
    }

    xSemaphoreGive(m_mutex);
    return err == ESP_OK;
}

void MqttConfigInterface::printStatus() {
    MqttConfigInfo_st cfg{};
    if (!hasConfig(&cfg)) {
        printf("MQTT: not provisioned\n");
        printf("  Configure with: mqtt_set <host> [port]\n");
        return;
    }

    printf("MQTT configuration:\n");
    printf("  broker      : %s:%u\n", cfg.host, (unsigned)cfg.port);
    printf("  username    : %s\n", cfg.username[0] ? cfg.username : "(anonymous)");
    printf("  password    : %s\n", cfg.password[0] ? "[CONFIGURED]" : "(none)");
    printf("  device id   : %s\n", cfg.device_id);
    printf("  name        : %s\n", cfg.name);
    printf("  room        : %s\n", cfg.room[0] ? cfg.room : "(unset)");
    printf("  ota url     : %s\n", cfg.ota_url[0] ? cfg.ota_url : "(unset)");
}

bool MqttConfigInterface::registerConsoleCommands() {
    esp_console_cmd_t cmd;

    cmd = {
        .command = "mqtt_set",
        .help = "Set MQTT broker\nUsage: mqtt_set <host> [port]",
        .hint = NULL,
        .func = &MqttConfigInterface::consoleSet,
        .argtable = NULL
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));

    cmd = {
        .command = "mqtt_auth",
        .help = "Set MQTT credentials\nUsage: mqtt_auth <username> <password>",
        .hint = NULL,
        .func = &MqttConfigInterface::consoleAuth,
        .argtable = NULL
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));

    cmd = {
        .command = "mqtt_device",
        .help = "Set device identity\nUsage: mqtt_device <device_id> <name> [room]\n"
                "device_id: lowercase letters, digits, '-' and '_' only",
        .hint = NULL,
        .func = &MqttConfigInterface::consoleDevice,
        .argtable = NULL
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));

    cmd = {
        .command = "mqtt_ota_url",
        .help = "Set the default OTA image URL\nUsage: mqtt_ota_url <url>",
        .hint = NULL,
        .func = &MqttConfigInterface::consoleOtaUrl,
        .argtable = NULL
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));

    cmd = {
        .command = "mqtt_status",
        .help = "Show MQTT configuration",
        .hint = NULL,
        .func = &MqttConfigInterface::consoleStatus,
        .argtable = NULL
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));

    cmd = {
        .command = "mqtt_clear",
        .help = "Erase MQTT configuration",
        .hint = NULL,
        .func = &MqttConfigInterface::consoleClear,
        .argtable = NULL
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));

    ESP_LOGI(TAG, "Console commands registered");
    return true;
}

int MqttConfigInterface::consoleSet(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: mqtt_set <host> [port]\n");
        return 1;
    }
    const uint16_t port = (argc >= 3) ? (uint16_t)atoi(argv[2]) : 1883;
    const bool ok = getInstance().setHost(argv[1], port);
    printf("%s\n", ok ? "MQTT broker saved. Reboot to apply." : "Failed to save broker.");
    return ok ? 0 : 1;
}

int MqttConfigInterface::consoleAuth(int argc, char** argv) {
    if (argc < 3) {
        printf("Usage: mqtt_auth <username> <password>\n");
        return 1;
    }
    const bool ok = getInstance().setCredentials(argv[1], argv[2]);
    printf("%s\n", ok ? "MQTT credentials saved. Reboot to apply."
                      : "Failed to save credentials.");
    return ok ? 0 : 1;
}

int MqttConfigInterface::consoleDevice(int argc, char** argv) {
    if (argc < 3) {
        printf("Usage: mqtt_device <device_id> <name> [room]\n");
        return 1;
    }
    if (!isValidDeviceId(argv[1])) {
        // Rejected rather than sanitised: this string becomes both an MQTT
        // topic segment and the Home Assistant unique_id, and a stray '/' or
        // uppercase letter produces discovery topics that silently never match.
        printf("Invalid device_id. Use lowercase letters, digits, '-' and '_' only.\n");
        return 1;
    }
    const char* room = (argc >= 4) ? argv[3] : nullptr;
    const bool ok = getInstance().setDevice(argv[1], argv[2], room);
    printf("%s\n", ok ? "Device identity saved. Reboot to apply."
                      : "Failed to save device identity.");
    return ok ? 0 : 1;
}

int MqttConfigInterface::consoleOtaUrl(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: mqtt_ota_url <url>\n");
        return 1;
    }
    const bool ok = getInstance().setOtaUrl(argv[1]);
    printf("%s\n", ok ? "OTA URL saved." : "Failed to save OTA URL.");
    return ok ? 0 : 1;
}

int MqttConfigInterface::consoleStatus(int, char**) {
    getInstance().printStatus();
    return 0;
}

int MqttConfigInterface::consoleClear(int, char**) {
    const bool ok = getInstance().clear();
    printf("%s\n", ok ? "MQTT configuration erased. Reboot to apply."
                      : "Failed to erase configuration.");
    return ok ? 0 : 1;
}
