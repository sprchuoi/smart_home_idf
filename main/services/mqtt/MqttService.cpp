/**
 * @file MqttService.cpp
 * @brief MQTT client for Smart_Server
 */

#include "MqttService.h"
#include "error/ErrorHandler.h"

#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "esp_wifi.h"

#include <cstdio>
#include <cstring>

const char* MqttService::TAG = "MqttService";

MqttService::MqttService() = default;

MqttService::~MqttService() {
    stop();
}

std::string MqttService::topicFor(const char* suffix) const {
    std::string topic(TOPIC_PREFIX);
    topic += '/';
    topic += m_cfg.device_id;
    if (suffix != nullptr && suffix[0] != '\0') {
        topic += '/';
        topic += suffix;
    }
    return topic;
}

bool MqttService::initialize(const MqttConfigInfo_st* cfg) {
    if (m_client != nullptr) {
        return true;
    }
    if (cfg == nullptr || cfg->host[0] == '\0' || cfg->device_id[0] == '\0') {
        ESP_LOGE(TAG, "Refusing to initialize without host and device id");
        return false;
    }

    m_cfg = *cfg;

    m_status_topic   = topicFor("status");
    m_command_topic  = topicFor("command");
    m_response_topic = topicFor("response");
    m_sensor_prefix  = topicFor("sensor") + "/";

    esp_mqtt_client_config_t mqtt_cfg = {};

    mqtt_cfg.broker.address.hostname  = m_cfg.host;
    mqtt_cfg.broker.address.port      = m_cfg.port;
    mqtt_cfg.broker.address.transport = MQTT_TRANSPORT_OVER_TCP;

    mqtt_cfg.credentials.client_id = m_cfg.device_id;
    if (m_cfg.username[0] != '\0') {
        mqtt_cfg.credentials.username = m_cfg.username;
        mqtt_cfg.credentials.authentication.password = m_cfg.password;
    }

    // Keepalive 30 s => the broker declares us dead at ~45 s if we vanish
    // without a clean disconnect.
    mqtt_cfg.session.keepalive = 30;

    // A persistent session means commands published while this node is
    // rebooting are queued by the broker instead of lost. The response topic
    // is correspondingly not retained, so a stale acknowledgement cannot be
    // replayed to the server on the next connect.
    mqtt_cfg.session.disable_clean_session = true;

    // The server tracks device.status, so the Last Will publishes an offline
    // *status document* rather than a bare string. Without this the server
    // would keep showing the node as online after it lost power.
    static const char* const OFFLINE_STATUS = "{\"status\":\"offline\"}";
    mqtt_cfg.session.last_will.topic   = m_status_topic.c_str();
    mqtt_cfg.session.last_will.msg     = OFFLINE_STATUS;
    mqtt_cfg.session.last_will.msg_len = (int)strlen(OFFLINE_STATUS);
    mqtt_cfg.session.last_will.qos     = 1;
    mqtt_cfg.session.last_will.retain  = 1;

    // We own the reconnect loop; see the header for why.
    mqtt_cfg.network.disable_auto_reconnect = true;
    mqtt_cfg.network.timeout_ms = 10000;

    // Discovery payloads carry a device block and run a few hundred bytes.
    mqtt_cfg.buffer.size     = 2048;
    mqtt_cfg.buffer.out_size = 2048;

    mqtt_cfg.task.stack_size = TASK_STACK_SIZE;
    mqtt_cfg.task.priority   = TASK_PRIORITY;

    m_client = esp_mqtt_client_init(&mqtt_cfg);
    if (m_client == nullptr) {
        ESP_LOGE(TAG, "esp_mqtt_client_init failed");
        return false;
    }

    esp_mqtt_client_register_event(m_client,
                                   static_cast<esp_mqtt_event_id_t>(ESP_EVENT_ANY_ID),
                                   eventHandler, this);

    if (xTaskCreatePinnedToCore(taskEntry, "MqttService", TASK_STACK_SIZE, this,
                                TASK_PRIORITY, &m_task, TASK_CORE) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create MQTT task");
        esp_mqtt_client_destroy(m_client);
        m_client = nullptr;
        return false;
    }

    ESP_LOGI(TAG, "Initialized for %s:%u as '%s'",
             m_cfg.host, (unsigned)m_cfg.port, m_cfg.device_id);
    return true;
}

void MqttService::start() {
    if (m_client == nullptr || m_started) {
        return;
    }
    m_started = true;
    if (m_task != nullptr) {
        xTaskNotifyGive(m_task);
    }
}

void MqttService::notifyWifiDown() {
    if (m_client != nullptr && m_connected.load()) {
        // Disconnect explicitly so the broker publishes our Last Will now,
        // rather than waiting out the keepalive.
        esp_mqtt_client_disconnect(m_client);
    }
}

bool MqttService::publish(const char* topic, const char* payload, int qos, int retain) {
    if (!m_connected.load() || m_client == nullptr) {
        return false;
    }
    const int msg_id = esp_mqtt_client_publish(m_client, topic, payload,
                                               payload ? (int)strlen(payload) : 0,
                                               qos, retain);
    if (msg_id < 0) {
        ESP_LOGW(TAG, "Publish to %s failed", topic);
        return false;
    }
    return true;
}

bool MqttService::publishSensor(const char* channel, float value, const char* unit) {
    const std::string topic = m_sensor_prefix + channel;
    // {"value": n, "unit": "u"} is the shape the server's bridge stores; it
    // reads `value` as a float and `unit` as a label.
    char payload[64];
    snprintf(payload, sizeof(payload), "{\"value\":%.2f,\"unit\":\"%s\"}",
             (double)value, unit != nullptr ? unit : "");
    return publish(topic.c_str(), payload, 0, 0);
}

bool MqttService::publishStatus(const char* json) {
    return publish(m_status_topic.c_str(), json, 1, 1);
}

bool MqttService::publishResponse(const char* json) {
    return publish(m_response_topic.c_str(), json, 1, 0);
}

void MqttService::publishDeviceStatus() {
    // The document Smart_Server keys off. Its bridge reads "status" for
    // online/offline; on first sight it additionally reads "device_type" and
    // "name" to register the device, and on later messages "firmware_version",
    // "ip" and "rssi" to update it.
    wifi_ap_record_t ap = {};
    const int rssi = (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) ? ap.rssi : 0;

    char ip[16] = {};
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif != nullptr) {
        esp_netif_ip_info_t ip_info = {};
        if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
            esp_ip4addr_ntoa(&ip_info.ip, ip, sizeof(ip));
        }
    }

    char status[320];
    snprintf(status, sizeof(status),
             "{\"status\":\"online\",\"device_type\":\"sensor_node\","
             "\"name\":\"%s\",\"firmware_version\":\"%s\",\"ip\":\"%s\","
             "\"room\":\"%s\",\"uptime_s\":%lld,\"heap\":%u,\"rssi\":%d}",
             m_cfg.name,
             esp_app_get_description() ? esp_app_get_description()->version : "unknown",
             ip,
             m_cfg.room,
             (long long)(esp_timer_get_time() / 1000000),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             rssi);
    publishStatus(status);
}

void MqttService::onConnected() {
    m_backoff_ms = INITIAL_BACKOFF_MS;

    // Birth message.
    publishDeviceStatus();

    // Home Assistant discovery is deliberately not published. Smart_Server
    // does not run Home Assistant -- it is a FastAPI stack with its own
    // database -- so discovery topics would be retained noise on its broker
    // with nothing to consume them. It returns in the phase that settles the
    // Google Home path; see ROADMAP.md.

    // QoS 1, with a persistent session, so a command sent while this node was
    // rebooting is queued by the broker rather than lost.
    esp_mqtt_client_subscribe(m_client, m_command_topic.c_str(), 1);

    ESP_LOGI(TAG, "Connected to %s:%u", m_cfg.host, (unsigned)m_cfg.port);
}

void MqttService::eventHandler(void* handler_args, esp_event_base_t, int32_t event_id,
                               void* event_data) {
    MqttService* self = static_cast<MqttService*>(handler_args);
    auto* event = static_cast<esp_mqtt_event_handle_t>(event_data);

    switch (static_cast<esp_mqtt_event_id_t>(event_id)) {
        case MQTT_EVENT_CONNECTED:
            self->m_connected.store(true);
            self->onConnected();
            break;

        case MQTT_EVENT_DISCONNECTED:
            self->m_connected.store(false);
            // Hand the reconnect to the service task: esp-mqtt documents that
            // client APIs other than publish/subscribe must not be called from
            // inside this handler.
            if (self->m_task != nullptr) {
                xTaskNotifyGive(self->m_task);
            }
            break;

        case MQTT_EVENT_DATA: {
            // topic/data point into esp-mqtt's buffer, are not NUL-terminated,
            // and are valid only for the duration of this callback.
            if (self->m_command_cb) {
                self->m_command_cb(event->topic, (size_t)event->topic_len,
                                   event->data, (size_t)event->data_len);
            }
            break;
        }

        case MQTT_EVENT_ERROR:
            self->m_connected.store(false);
            ErrorHandler::getInstance().reportError(
                ErrorCategory::MQTT_ERROR, -1, "transport error");
            break;

        default:
            break;
    }
}

void MqttService::taskLoop() {
    ESP_LOGI(TAG, "MQTT task on core %d", xPortGetCoreID());
    esp_task_wdt_add(NULL);

    // Wait for start(). Bounded waits so we keep feeding the watchdog and can
    // notice shutdown while WiFi is still coming up.
    while (!m_started) {
        esp_task_wdt_reset();
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
        if (m_stopping.load()) {
            esp_task_wdt_delete(NULL);
            vTaskDelete(nullptr);
            return;
        }
    }

    esp_mqtt_client_start(m_client);

    for (;;) {
        esp_task_wdt_reset();
        if (m_stopping.load()) {
            break;
        }

        // The event handler notifies on every disconnect.
        if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000)) == pdTRUE) {
            if (m_stopping.load()) {
                break;
            }
            // Exponential backoff with +-10% jitter, so a broker restart does
            // not have every node in the house retrying in lockstep.
            const int jitter = (int)(esp_random() % (uint32_t)(m_backoff_ms / 5 + 1));
            const int wait_ms = m_backoff_ms - m_backoff_ms / 10 + jitter;
            ESP_LOGW(TAG, "Reconnect in %d ms", wait_ms);
            vTaskDelay(pdMS_TO_TICKS(wait_ms));

            // Returns ESP_FAIL unless the client is parked in WAIT_RECONNECT,
            // which is benign -- a reconnect may already be in flight.
            esp_mqtt_client_reconnect(m_client);

            m_backoff_ms = (m_backoff_ms < MAX_BACKOFF_MS) ? m_backoff_ms * 2 : MAX_BACKOFF_MS;
        }
    }

    esp_task_wdt_delete(NULL);
    vTaskDelete(nullptr);
}

void MqttService::taskEntry(void* parameter) {
    static_cast<MqttService*>(parameter)->taskLoop();
}

void MqttService::stop() {
    m_stopping.store(true);
    m_started = false;

    if (m_client != nullptr) {
        esp_mqtt_client_stop(m_client);
        esp_mqtt_client_destroy(m_client);
        m_client = nullptr;
    }
    m_connected.store(false);

    if (m_task != nullptr) {
        // The task deletes itself once it observes m_stopping.
        m_task = nullptr;
    }
}
