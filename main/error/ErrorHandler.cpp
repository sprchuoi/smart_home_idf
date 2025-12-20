/**
 * @file ErrorHandler.cpp
 * @brief Error Handler implementation
 */

#include "ErrorHandler.h"
#include <cstdarg>

const char* ErrorHandler::TAG = "ErrorHandler";

ErrorHandler& ErrorHandler::getInstance() {
    static ErrorHandler instance;
    return instance;
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

    ESP_LOGE(TAG, "[%d] Error %d: %s",
             static_cast<int>(category),
             error_code,
             buffer);

    EventMessage event{};
    event.type = EventType::SYSTEM_ERROR;
    event.source = EventSource::ERROR_HANDLER;
    event.destination = EventSource::APPLICATION;
    event.payload.error_info.error_code = error_code;

    strncpy(event.payload.error_info.error_msg,
            buffer,
            sizeof(event.payload.error_info.error_msg) - 1);

    EventBus::getInstance().publish(event);
}
