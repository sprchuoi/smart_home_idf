/**
 * @file ErrorHandler.h
 * @brief Central error handling system
 */

#pragma once

#include "core/eventbus/EventBus.h"
#include <string>

/**
 * @brief Error categories
 */
enum class ErrorCategory : uint8_t {
    WIFI_ERROR,
    MQTT_ERROR,
    DISPLAY_ERROR,
    AUDIO_ERROR,
    SYSTEM_ERROR,
    NVS_ERROR,
    POWER_ERROR,
    OTA_ERROR,
    UART_ERROR
};

/**
 * @brief Error Handler class
 * 
 * Centralized error reporting and propagation via EventBus
 */
class ErrorHandler {
public:
    static ErrorHandler& getInstance();
    
    /**
     * @brief Report an error
     * @param category Error category
     * @param error_code Error code
     * @param error_msg Error message
     */
    void reportError(ErrorCategory category,
                        int error_code,
                        const char* format,
                        ...);

private:
    ErrorHandler() = default;
    ~ErrorHandler() = default;
    ErrorHandler(const ErrorHandler&) = delete;
    ErrorHandler& operator=(const ErrorHandler&) = delete;
    
    static const char* TAG;
};

