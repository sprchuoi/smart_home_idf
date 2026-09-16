/**
 * @file ErrorHandler.cpp
 * @brief Error Handler implementation
 */

#include "ErrorHandler.h"
#include "esp_log.h"
#include <cstdarg>
#include <cstdio>

const char* ErrorHandler::TAG = "ErrorHandler";

ErrorHandler& ErrorHandler::getInstance() {
    static ErrorHandler instance;
    return instance;
}

const char* ErrorHandler::categoryName(ErrorCategory category) {
    switch (category) {
        case ErrorCategory::WIFI_ERROR:   return "WIFI";
        case ErrorCategory::MQTT_ERROR:   return "MQTT";
        case ErrorCategory::SYSTEM_ERROR: return "SYSTEM";
        case ErrorCategory::NVS_ERROR:    return "NVS";
        case ErrorCategory::OTA_ERROR:    return "OTA";
        case ErrorCategory::UART_ERROR:   return "UART";
    }
    return "UNKNOWN";
}

void ErrorHandler::reportError(ErrorCategory category,
                               int error_code,
                               const char* format,
                               ...)
{
    char buffer[256];

    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    ++m_error_count;
    const size_t idx = static_cast<size_t>(category);
    if (idx < CATEGORY_COUNT) {
        ++m_category_counts[idx];
    }

    ESP_LOGE(TAG, "[%s] error %d: %s",
             categoryName(category),
             error_code,
             buffer);
}

uint32_t ErrorHandler::getErrorCount(ErrorCategory category) const {
    const size_t idx = static_cast<size_t>(category);
    return (idx < CATEGORY_COUNT) ? m_category_counts[idx] : 0;
}
