/**
 * @file AppStateMachine.h
 * @brief Application state tracking
 *
 * This was a 294-line state machine with its own FreeRTOS task, an 8-state
 * transition table, and 9 EventBus subscriptions. None of it ever ran, and
 * none of it could: it waited on WIFI_STARTED and WIFI_GOT_IP, which no code
 * published, so it would have sat in INIT forever.
 *
 * What is actually useful is the state itself -- it is what the MQTT status
 * topic reports. That is all this is now. No task, no queue, no bus.
 */

#pragma once

#include <atomic>
#include <cstdint>

/**
 * @brief Application states
 */
enum class AppState : uint8_t {
    INIT,
    WIFI_CONNECTING,
    WIFI_CONNECTED,
    MQTT_CONNECTING,
    RUNNING,
    OTA_UPDATING,
    ERROR
};

/**
 * @brief Application state holder
 *
 * Written by the service callbacks (WiFi link events, MQTT connect events) and
 * read by whatever reports status. Atomic because those callbacks run in
 * different task contexts.
 */
class AppStateMachine {
public:
    /**
     * @brief Set the current state, logging transitions.
     */
    void setState(AppState state);

    /**
     * @brief Get the current state.
     */
    AppState getState() const { return m_state.load(std::memory_order_relaxed); }

    /**
     * @brief Get the current state as a string.
     */
    const char* getStateString() const { return toString(getState()); }

    /**
     * @brief Human-readable state name.
     */
    static const char* toString(AppState state);

private:
    std::atomic<AppState> m_state{AppState::INIT};
};
