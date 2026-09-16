/**
 * @file AppStateMachine.cpp
 * @brief Application state tracking
 */

#include "AppStateMachine.h"
#include "esp_log.h"

static const char* TAG = "AppState";

const char* AppStateMachine::toString(AppState state) {
    switch (state) {
        case AppState::INIT:            return "INIT";
        case AppState::WIFI_CONNECTING: return "WIFI_CONNECTING";
        case AppState::WIFI_CONNECTED:  return "WIFI_CONNECTED";
        case AppState::MQTT_CONNECTING: return "MQTT_CONNECTING";
        case AppState::RUNNING:         return "RUNNING";
        case AppState::OTA_UPDATING:    return "OTA_UPDATING";
        case AppState::ERROR:           return "ERROR";
    }
    return "UNKNOWN";
}

void AppStateMachine::setState(AppState state) {
    const AppState previous = m_state.exchange(state, std::memory_order_relaxed);
    if (previous != state) {
        ESP_LOGI(TAG, "%s -> %s", toString(previous), toString(state));
    }
}
