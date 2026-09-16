/**
 * @file ErrorHandler.h
 * @brief Central error handling system
 */

#pragma once

#include <cstdint>
#include <cstddef>

/**
 * @brief Error categories
 *
 * DISPLAY and AUDIO were dropped along with those subsystems. The categories
 * exist so a call site can say *what* failed -- the message alone rarely makes
 * that obvious when reading a log.
 */
enum class ErrorCategory : uint8_t {
    WIFI_ERROR,
    MQTT_ERROR,
    SYSTEM_ERROR,
    NVS_ERROR,
    OTA_ERROR,
    UART_ERROR
};

/**
 * @brief Error Handler class
 *
 * Centralized error reporting. Logs each error with its category and keeps a
 * running count, so a node that is failing quietly can be diagnosed from its
 * status topic rather than only from a serial log nobody is watching.
 *
 * It previously also published to the EventBus, which had no consumers -- every
 * error went into a queue that was never drained.
 */
class ErrorHandler {
public:
    static ErrorHandler& getInstance();

    /**
     * @brief Report an error
     * @param category Error category
     * @param error_code Error code
     * @param format printf-style message
     */
    void reportError(ErrorCategory category,
                     int error_code,
                     const char* format,
                     ...) __attribute__((format(printf, 4, 5)));

    /**
     * @brief Total errors reported since boot.
     */
    uint32_t getErrorCount() const { return m_error_count; }

    /**
     * @brief Errors reported since boot in one category.
     */
    uint32_t getErrorCount(ErrorCategory category) const;

    /**
     * @brief Human-readable category name, for logs and the status topic.
     */
    static const char* categoryName(ErrorCategory category);

private:
    ErrorHandler() = default;
    ~ErrorHandler() = default;
    ErrorHandler(const ErrorHandler&) = delete;
    ErrorHandler& operator=(const ErrorHandler&) = delete;

    static constexpr size_t CATEGORY_COUNT = 6;

    uint32_t m_error_count = 0;
    uint32_t m_category_counts[CATEGORY_COUNT] = {};

    static const char* TAG;
};
