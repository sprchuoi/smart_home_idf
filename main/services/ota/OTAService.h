/**
 * @file OTAService.h
 * @brief HTTPS OTA update service
 *
 * Writes to the inactive OTA slot and reboots into it. Requires the dual-slot
 * partition table now in partitions.csv -- the previous single `factory`
 * partition meant this could never have worked.
 *
 * Progress is reported through a callback rather than the removed EventBus.
 */

#pragma once

#include "esp_https_ota.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <atomic>
#include <functional>
#include <string>

/// Called from the OTA task as the image downloads. Keep it short.
using OtaProgressCallback = std::function<void(int percent)>;

class OTAService {
public:
    OTAService();
    ~OTAService();

    bool initialize();

    void setProgressCallback(OtaProgressCallback cb) { m_progress_cb = std::move(cb); }

    /**
     * @brief Request an update. Returns immediately; the OTA task does the work.
     */
    bool requestUpdate(const char* url);

    bool isOTAInProgress() const { return m_ota_in_progress.load(); }

    void stop();

    static void taskEntry(void* parameter);

private:
    void taskLoop();
    void runUpdate(const std::string& url);
    void reportProgress(int percent);

    TaskHandle_t m_task_handle = nullptr;
    SemaphoreHandle_t m_ota_mutex = nullptr;
    bool m_initialized = false;
    std::atomic<bool> m_stopping{false};
    std::atomic<bool> m_ota_in_progress{false};
    std::string m_ota_url;
    OtaProgressCallback m_progress_cb;

    static const char* TAG;
    static constexpr int TASK_STACK_SIZE = 8192;
    static constexpr int TASK_PRIORITY = 6;
    static constexpr BaseType_t TASK_CORE = 0;
};
