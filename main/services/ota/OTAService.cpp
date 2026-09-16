/**
 * @file OTAService.cpp
 * @brief HTTPS OTA update implementation
 */

#include "OTAService.h"
#include "error/ErrorHandler.h"

#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_task_wdt.h"

#include <cstring>

const char* OTAService::TAG = "OTAService";

OTAService::OTAService() = default;

OTAService::~OTAService() {
    stop();
}

bool OTAService::initialize() {
    if (m_initialized) {
        return true;
    }

    m_ota_mutex = xSemaphoreCreateMutex();
    if (m_ota_mutex == nullptr) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return false;
    }

    if (xTaskCreatePinnedToCore(taskEntry, "OTAService", TASK_STACK_SIZE, this,
                                TASK_PRIORITY, &m_task_handle, TASK_CORE) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create OTA task");
        vSemaphoreDelete(m_ota_mutex);
        m_ota_mutex = nullptr;
        return false;
    }

    m_initialized = true;
    ESP_LOGI(TAG, "OTA service initialized");
    return true;
}

bool OTAService::requestUpdate(const char* url) {
    if (!m_initialized || url == nullptr || url[0] == '\0') {
        return false;
    }
    if (m_ota_in_progress.load()) {
        ESP_LOGW(TAG, "An update is already in progress");
        return false;
    }

    if (xSemaphoreTake(m_ota_mutex, portMAX_DELAY) != pdTRUE) {
        return false;
    }
    m_ota_url = url;
    m_ota_in_progress.store(true);
    xSemaphoreGive(m_ota_mutex);

    ESP_LOGI(TAG, "Update requested: %s", url);
    return true;
}

void OTAService::reportProgress(int percent) {
    if (m_progress_cb) {
        m_progress_cb(percent);
    }
}

void OTAService::runUpdate(const std::string& url) {
    esp_http_client_config_t http = {};
    http.url = url.c_str();
    http.keep_alive_enable = true;
    // No total-transfer timeout. The 5 s this used to set would have aborted
    // every real download: it caps the whole body, not one read.
    http.timeout_ms = 0;

    esp_https_ota_config_t ota_config = {};
    ota_config.http_config = &http;
    ota_config.partial_http_download = true;

    esp_https_ota_handle_t handle = nullptr;
    esp_err_t err = esp_https_ota_begin(&ota_config, &handle);
    if (err != ESP_OK || handle == nullptr) {
        ESP_LOGE(TAG, "esp_https_ota_begin failed: %s", esp_err_to_name(err));
        ErrorHandler::getInstance().reportError(ErrorCategory::OTA_ERROR, err,
                                                "OTA begin failed");
        return;
    }

    esp_app_desc_t incoming = {};
    if (esp_https_ota_get_img_desc(handle, &incoming) == ESP_OK) {
        const esp_app_desc_t* running = esp_app_get_description();
        ESP_LOGI(TAG, "Updating %s -> %s",
                 running ? running->version : "?", incoming.version);
    }

    int last_reported = -1;
    while ((err = esp_https_ota_perform(handle)) == ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
        const int total = esp_https_ota_get_image_size(handle);
        if (total > 0) {
            const int percent = (int)((esp_https_ota_get_image_len_read(handle) * 100) / total);
            if (percent != last_reported) {
                last_reported = percent;
                reportProgress(percent);
            }
        }
        esp_task_wdt_reset();
    }

    if (err != ESP_OK || !esp_https_ota_is_complete_data_received(handle)) {
        ESP_LOGE(TAG, "OTA failed: %s", esp_err_to_name(err));
        ErrorHandler::getInstance().reportError(ErrorCategory::OTA_ERROR, err,
                                                "OTA transfer failed");
        reportProgress(0);
        esp_https_ota_abort(handle);
        return;
    }

    err = esp_https_ota_finish(handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_https_ota_finish failed: %s", esp_err_to_name(err));
        ErrorHandler::getInstance().reportError(ErrorCategory::OTA_ERROR, err,
                                                "OTA finish failed");
        reportProgress(0);
        return;
    }

    ESP_LOGI(TAG, "Update written; rebooting");
    reportProgress(100);
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
}

void OTAService::taskLoop() {
    ESP_LOGI(TAG, "OTA task on core %d", xPortGetCoreID());
    esp_task_wdt_add(NULL);

    while (!m_stopping.load()) {
        esp_task_wdt_reset();

        if (m_ota_in_progress.load()) {
            std::string url;
            if (xSemaphoreTake(m_ota_mutex, portMAX_DELAY) == pdTRUE) {
                url = m_ota_url;
                xSemaphoreGive(m_ota_mutex);
            }

            if (!url.empty()) {
                runUpdate(url);
            }
            // Reached only on failure -- success reboots.
            m_ota_in_progress.store(false);
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }

    esp_task_wdt_delete(NULL);
    vTaskDelete(nullptr);
}

void OTAService::taskEntry(void* parameter) {
    static_cast<OTAService*>(parameter)->taskLoop();
}

void OTAService::stop() {
    m_stopping.store(true);
    if (m_task_handle != nullptr) {
        // The task deletes itself once it observes m_stopping.
        m_task_handle = nullptr;
    }
    if (m_ota_mutex != nullptr) {
        vSemaphoreDelete(m_ota_mutex);
        m_ota_mutex = nullptr;
    }
    m_initialized = false;
    m_ota_in_progress.store(false);
}
