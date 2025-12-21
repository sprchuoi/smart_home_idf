/**
 * @file WifiConfigService.cpp
 * @brief WiFi Configuration Service implementation
 */

#include "WifiConfigService.h"
#include <cstring>

const char* WifiConfigService::TAG = "WifiConfigService";
const char* WifiConfigService::NVS_NAMESPACE = "wifi_config";
const char* WifiConfigService::KEY_SSID = "ssid";
const char* WifiConfigService::KEY_PASSWORD = "password";

WifiConfigService& WifiConfigService::getInstance() {
    static WifiConfigService instance;
    return instance;
}

bool WifiConfigService::initialize() {
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
    
    m_initialized = true;
    ESP_LOGI(TAG, "WifiConfigService initialized");
    return true;
}

bool WifiConfigService::getSSID(char* ssid, size_t max_len) {
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

bool WifiConfigService::getPassword(char* password, size_t max_len) {
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

bool WifiConfigService::setSSID(const char* ssid) {
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
    
    ESP_LOGI(TAG, "SSID saved to NVS");
    return true;
}

bool WifiConfigService::setPassword(const char* password) {
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
    
    ESP_LOGI(TAG, "Password saved to NVS");
    return true;
}

bool WifiConfigService::hasCredentials(WifiConfigInfo_st *m_wifi_cfg) {
    if (!m_initialized) {
        return false;
    }
    return getSSID(m_wifi_cfg->ssid, sizeof(m_wifi_cfg->ssid)) && getPassword(m_wifi_cfg->password, sizeof(m_wifi_cfg->password));
}

void WifiConfigService::deinitialize() {
    if (m_initialized) {
        nvs_close(m_nvs_handle);
        m_initialized = false;
    }
    
    if (m_mutex != nullptr) {
        vSemaphoreDelete(m_mutex);
        m_mutex = nullptr;
    }
    
    ESP_LOGI(TAG, "WifiConfigService deinitialized");
}

