#pragma once

#include <cstdint>

// Every buffer is MAX_LEN + 1 so a value of exactly MAX_LEN characters still
// fits with its NUL terminator. Wifi_cfg.hpp gets this wrong -- its
// `char ssid[32]` cannot hold a legal 32-character SSID -- which is why these
// are declared this way.
#define MQTT_HOST_MAX_LEN     64
#define MQTT_USER_MAX_LEN     32
#define MQTT_PASS_MAX_LEN     64
#define MQTT_DEVID_MAX_LEN    24   // becomes an MQTT topic segment AND an HA identifier
#define MQTT_NAME_MAX_LEN     32   // HA device name
#define MQTT_ROOM_MAX_LEN     24   // HA suggested_area
#define MQTT_OTA_URL_MAX_LEN  192

// Longest topic this firmware builds. 6 (prefix) + DEVID + 1 + 20 (longest
// suffix: "availability") + 1 is well under this.
#define MQTT_TOPIC_MAX_LEN    160

/**
 * @brief MQTT broker identity and this node's identity on it.
 *
 * Plain POD so it can be read from NVS in one pass and passed to the service by
 * pointer, with no allocation and no lifetime questions.
 *
 * Empty host means "not provisioned" -- the node boots to a usable console
 * instead of retrying a broker that was never configured.
 */
struct MqttConfigInfo_st {
    char     host[MQTT_HOST_MAX_LEN + 1];
    uint16_t port;
    char     username[MQTT_USER_MAX_LEN + 1];   // empty => anonymous
    char     password[MQTT_PASS_MAX_LEN + 1];
    char     device_id[MQTT_DEVID_MAX_LEN + 1]; // empty => filled in from the MAC
    char     name[MQTT_NAME_MAX_LEN + 1];
    char     room[MQTT_ROOM_MAX_LEN + 1];
    char     ota_url[MQTT_OTA_URL_MAX_LEN + 1]; // optional default image URL
};
