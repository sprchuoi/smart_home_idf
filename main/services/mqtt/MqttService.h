/**
 * @file MqttService.h
 * @brief MQTT client for Home Assistant
 *
 * Runs on Core 0. Publishes availability, status and per-channel telemetry,
 * announces itself to Home Assistant via MQTT discovery, and dispatches
 * commands to a registered callback.
 *
 * Reconnect policy: esp-mqtt's built-in auto-reconnect is a *fixed* 10 s with
 * no backoff (MQTT_RECON_DEFAULT_MS in mqtt_client.c), so it is disabled and
 * this service owns the loop. It retries with exponential backoff and jitter,
 * capped at 60 s.
 */

#pragma once

#include "cfg/Mqtt_cfg.hpp"
#include "mqtt_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <atomic>
#include <cstddef>
#include <functional>
#include <string>

/**
 * @brief Command callback.
 *
 * Invoked from the esp-mqtt task. `topic` and `data` point into esp-mqtt's own
 * buffer: they are NOT NUL-terminated and are valid only for the duration of
 * the call. Copy anything you intend to keep, and do not call esp-mqtt APIs
 * other than publish/subscribe from inside it.
 */
using MqttCommandCallback = std::function<void(const char* topic, size_t topic_len,
                                               const char* data, size_t data_len)>;

class MqttService {
public:
    MqttService();
    ~MqttService();

    /**
     * @brief Configure the client. Does not connect.
     */
    bool initialize(const MqttConfigInfo_st* cfg);

    void setCommandCallback(MqttCommandCallback cb) { m_command_cb = std::move(cb); }

    /**
     * @brief Begin connecting. Call once WiFi has an address. Idempotent.
     */
    void start();

    /**
     * @brief Drop the connection so the broker publishes our Last Will now,
     *        rather than waiting out the keepalive (up to 45 s).
     */
    void notifyWifiDown();

    bool isConnected() const { return m_connected.load(std::memory_order_relaxed); }

    /**
     * @brief Publish one telemetry value: QoS 0, not retained.
     *
     * A stale temperature reading has no value, and QoS 1 telemetry on a node
     * whose broker is down just fills the outbox.
     */
    bool publishState(const char* channel, const char* value);

    /**
     * @brief Publish the device status document: QoS 1, retained.
     */
    bool publishStatus(const char* json);

    /**
     * @brief Publish availability: QoS 1, retained.
     */
    bool publishAvailability(bool online);

    void stop();

private:
    static void taskEntry(void* parameter);
    static void eventHandler(void* handler_args, esp_event_base_t base,
                             int32_t event_id, void* event_data);

    void taskLoop();
    void onConnected();
    void publishDiscovery();
    bool publish(const char* topic, const char* payload, int qos, int retain);
    std::string topicFor(const char* suffix) const;

    esp_mqtt_client_handle_t m_client = nullptr;
    TaskHandle_t m_task = nullptr;
    std::atomic<bool> m_connected{false};
    std::atomic<bool> m_stopping{false};
    bool m_started = false;
    int m_backoff_ms = 2000;

    MqttConfigInfo_st m_cfg{};
    std::string m_availability_topic;
    std::string m_status_topic;
    std::string m_command_topic;
    MqttCommandCallback m_command_cb;

    static const char* TAG;
    static constexpr const char* TOPIC_PREFIX = "smart_home";
    static constexpr int TASK_STACK_SIZE = 4096;
    static constexpr int TASK_PRIORITY = 5;
    static constexpr BaseType_t TASK_CORE = 0;  // networking lives on Core 0
    static constexpr int INITIAL_BACKOFF_MS = 2000;
    static constexpr int MAX_BACKOFF_MS = 60000;
};
