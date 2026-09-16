/**
 * @file MqttService.cpp
 * @brief MQTT client for Home Assistant
 */

#include "MqttService.h"
#include "error/ErrorHandler.h"

#include "cJSON.h"
#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "esp_wifi.h"

#include <cstdio>
#include <cstring>

const char* MqttService::TAG = "MqttService";

namespace {

/**
 * The Home Assistant `device` block shared by every entity, so they group under
 * one device card instead of appearing as unrelated entries.
 */
cJSON* makeDeviceBlock(const MqttConfigInfo_st& cfg) {
    cJSON* dev = cJSON_CreateObject();

    cJSON* identifiers = cJSON_CreateArray();
    cJSON_AddItemToArray(identifiers, cJSON_CreateString(cfg.device_id));
    cJSON_AddItemToObject(dev, "identifiers", identifiers);

    cJSON_AddStringToObject(dev, "name", cfg.name);
    cJSON_AddStringToObject(dev, "manufacturer", "sprchuoi");
    cJSON_AddStringToObject(dev, "model", "ESP32-S3 Smart Home Node");

    const esp_app_desc_t* app = esp_app_get_description();
    if (app != nullptr) {
        cJSON_AddStringToObject(dev, "sw_version", app->version);
    }

    // suggested_area is only honoured the first time an entity appears, which
    // is one reason discovery is republished on every connect.
    if (cfg.room[0] != '\0') {
        cJSON_AddStringToObject(dev, "suggested_area", cfg.room);
    }

    return dev;
}

}  // namespace

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

    m_availability_topic = topicFor("availability");
    m_status_topic       = topicFor("status");
    m_command_topic      = topicFor("cmd/#");

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
    // rebooting are queued by the broker instead of lost. It is also why
    // command publishing must not be retained -- see publishDiscovery().
    mqtt_cfg.session.disable_clean_session = true;

    // Without this the entity would advertise an availability topic that is
    // never published, so Home Assistant would show it as unavailable forever.
    mqtt_cfg.session.last_will.topic  = m_availability_topic.c_str();
    mqtt_cfg.session.last_will.msg    = "offline";
    mqtt_cfg.session.last_will.msg_len = 7;
    mqtt_cfg.session.last_will.qos    = 1;
    mqtt_cfg.session.last_will.retain = 1;

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

bool MqttService::publishState(const char* channel, const char* value) {
    const std::string topic = topicFor(channel) + "/state";
    return publish(topic.c_str(), value, 0, 0);
}

bool MqttService::publishStatus(const char* json) {
    return publish(m_status_topic.c_str(), json, 1, 1);
}

bool MqttService::publishAvailability(bool online) {
    return publish(m_availability_topic.c_str(), online ? "online" : "offline", 1, 1);
}

void MqttService::publishDiscovery() {
    // Announces the entities that need no sensor hardware, so the whole
    // pipeline can be proven end to end before Phase 3 adds real sensors.
    auto emitSensor = [&](const char* object_id, const char* name,
                          const char* state_topic, const char* device_class,
                          const char* unit, const char* state_class,
                          const char* entity_category) {
        cJSON* o = cJSON_CreateObject();

        const std::string unique = std::string(m_cfg.device_id) + "_" + object_id;
        cJSON_AddStringToObject(o, "name", name);
        cJSON_AddStringToObject(o, "unique_id", unique.c_str());
        cJSON_AddStringToObject(o, "object_id", unique.c_str());
        if (device_class != nullptr)   cJSON_AddStringToObject(o, "device_class", device_class);
        if (unit != nullptr)           cJSON_AddStringToObject(o, "unit_of_measurement", unit);
        if (state_class != nullptr)    cJSON_AddStringToObject(o, "state_class", state_class);
        if (entity_category != nullptr) cJSON_AddStringToObject(o, "entity_category", entity_category);
        cJSON_AddStringToObject(o, "state_topic", state_topic);
        cJSON_AddStringToObject(o, "availability_topic", m_availability_topic.c_str());
        cJSON_AddStringToObject(o, "payload_available", "online");
        cJSON_AddStringToObject(o, "payload_not_available", "offline");
        cJSON_AddItemToObject(o, "device", makeDeviceBlock(m_cfg));

        char* text = cJSON_PrintUnformatted(o);
        if (text != nullptr) {
            const std::string topic = std::string("homeassistant/sensor/") +
                                      m_cfg.device_id + "/" + object_id + "/config";
            publish(topic.c_str(), text, 1, 1);
            cJSON_free(text);
        }
        cJSON_Delete(o);
    };

    emitSensor("rssi", "WiFi Signal", (topicFor("rssi") + "/state").c_str(),
               "signal_strength", "dBm", "measurement", "diagnostic");
    emitSensor("heap", "Free Heap", (topicFor("heap") + "/state").c_str(),
               nullptr, "B", "measurement", "diagnostic");
    emitSensor("uptime", "Uptime", (topicFor("uptime") + "/state").c_str(),
               "duration", "s", "total_increasing", "diagnostic");

    // Connectivity, driven directly by the availability topic. This is the one
    // entity that reports whether the node itself is reachable.
    {
        cJSON* o = cJSON_CreateObject();
        const std::string unique = std::string(m_cfg.device_id) + "_link";
        cJSON_AddStringToObject(o, "name", "Link");
        cJSON_AddStringToObject(o, "unique_id", unique.c_str());
        cJSON_AddStringToObject(o, "object_id", unique.c_str());
        cJSON_AddStringToObject(o, "device_class", "connectivity");
        cJSON_AddStringToObject(o, "entity_category", "diagnostic");
        cJSON_AddStringToObject(o, "state_topic", m_availability_topic.c_str());
        cJSON_AddStringToObject(o, "payload_on", "online");
        cJSON_AddStringToObject(o, "payload_off", "offline");
        cJSON_AddStringToObject(o, "availability_topic", m_availability_topic.c_str());
        cJSON_AddStringToObject(o, "payload_available", "online");
        cJSON_AddStringToObject(o, "payload_not_available", "offline");
        cJSON_AddItemToObject(o, "device", makeDeviceBlock(m_cfg));

        char* text = cJSON_PrintUnformatted(o);
        if (text != nullptr) {
            const std::string topic = std::string("homeassistant/binary_sensor/") +
                                      m_cfg.device_id + "/link/config";
            publish(topic.c_str(), text, 1, 1);
            cJSON_free(text);
        }
        cJSON_Delete(o);
    }
}

void MqttService::onConnected() {
    m_backoff_ms = INITIAL_BACKOFF_MS;

    // Birth message. The Last Will only covers death -- without this, Home
    // Assistant gates every entity on an availability topic that would never
    // carry "online".
    publishAvailability(true);

    // Republished on every connect, not just the first: it is how changes to
    // name/room/software version propagate, and how a deleted retained topic
    // recovers.
    publishDiscovery();

    // QoS 1: a command sent while this node was rebooting must not be lost.
    esp_mqtt_client_subscribe(m_client, m_command_topic.c_str(), 1);

    // Status, retained, so it survives a broker restart.
    char status[256];
    wifi_ap_record_t ap = {};
    const int rssi = (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) ? ap.rssi : 0;
    snprintf(status, sizeof(status),
             "{\"fw\":\"%s\",\"uptime_s\":%lld,\"heap\":%u,\"rssi\":%d,\"reset_reason\":%d}",
             esp_app_get_description() ? esp_app_get_description()->version : "unknown",
             (long long)(esp_timer_get_time() / 1000000),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             rssi,
             (int)esp_reset_reason());
    publishStatus(status);

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
