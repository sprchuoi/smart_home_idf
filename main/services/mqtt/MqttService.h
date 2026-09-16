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
     * @brief Publish one sensor reading: QoS 0, not retained.
     *
     * Goes to smart_home/devices/<id>/sensor/<channel> as
     * {"value": <n>, "unit": "<unit>"}, which is the shape Smart_Server's
     * MQTT bridge stores.
     *
     * A stale reading has no value, and QoS 1 telemetry on a node whose broker
     * is unreachable just fills the outbox.
     */
    bool publishSensor(const char* channel, float value, const char* unit);

    /**
     * @brief Publish the device status document: QoS 1, retained.
     *
     * The payload must carry "status" and may carry "device_type", "name",
     * "firmware_version", "ip" and "rssi" -- those are the fields the server
     * reads to register and update a device.
     */
    bool publishStatus(const char* json);

    /**
     * @brief Acknowledge a command: QoS 1, not retained.
     */
    bool publishResponse(const char* json);

    /**
     * @brief Build and publish the retained status document.
     *
     * Sent automatically on connect; also served on demand so the server can
     * ask for it with a "get_status" command.
     */
    void publishDeviceStatus();

    void stop();

private:
    static void taskEntry(void* parameter);
    static void eventHandler(void* handler_args, esp_event_base_t base,
                             int32_t event_id, void* event_data);

    void taskLoop();
    void onConnected();
    bool publish(const char* topic, const char* payload, int qos, int retain);
    std::string topicFor(const char* suffix) const;

    esp_mqtt_client_handle_t m_client = nullptr;
    TaskHandle_t m_task = nullptr;
    std::atomic<bool> m_connected{false};
    std::atomic<bool> m_stopping{false};
    bool m_started = false;
    int m_backoff_ms = 2000;

    MqttConfigInfo_st m_cfg{};
    std::string m_status_topic;    // smart_home/devices/<id>/status
    std::string m_command_topic;   // smart_home/devices/<id>/command
    std::string m_response_topic;  // smart_home/devices/<id>/response
    std::string m_sensor_prefix;   // smart_home/devices/<id>/sensor/
    MqttCommandCallback m_command_cb;

    static const char* TAG;

    // Topic layout is dictated by Smart_Server's MQTT bridge, which parses
    // topics positionally: parts[2] is the device id and parts[3] the message
    // type. The "devices" segment is therefore load-bearing, not decoration.
    static constexpr const char* TOPIC_PREFIX = "smart_home/devices";
    static constexpr int TASK_STACK_SIZE = 4096;
    static constexpr int TASK_PRIORITY = 5;
    static constexpr BaseType_t TASK_CORE = 0;  // networking lives on Core 0
    static constexpr int INITIAL_BACKOFF_MS = 2000;
    static constexpr int MAX_BACKOFF_MS = 60000;
};
