/**
 * @file SystemCommands.cpp
 * @brief Device-level console commands
 */

#include "SystemCommands.h"
#include "core/console/Console.h"

#include "esp_app_desc.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cstdio>
#include <iterator>

namespace system_commands {
namespace {

const char* resetReasonName(esp_reset_reason_t reason) {
    switch (reason) {
        case ESP_RST_POWERON:   return "power-on";
        case ESP_RST_EXT:       return "external pin";
        case ESP_RST_SW:        return "software restart";
        case ESP_RST_PANIC:     return "panic";
        case ESP_RST_INT_WDT:   return "interrupt watchdog";
        case ESP_RST_TASK_WDT:  return "task watchdog";
        case ESP_RST_WDT:       return "other watchdog";
        case ESP_RST_DEEPSLEEP: return "deep-sleep wake";
        case ESP_RST_BROWNOUT:  return "brownout";
        case ESP_RST_SDIO:      return "SDIO";
        default:                return "unknown";
    }
}

int handleReboot(int, char**) {
    // Flush before resetting. Without this the message above can be lost with
    // the reset, and the console looks like it ignored the command.
    printf("Rebooting...\n");
    fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(100));

    esp_restart();
    return 0;  // not reached
}

int handleVersion(int, char**) {
    const esp_app_desc_t* app = esp_app_get_description();
    if (app != nullptr) {
        printf("Firmware : %s\n", app->version);
        printf("Built    : %s %s\n", app->date, app->time);
        printf("IDF      : %s\n", app->idf_ver);
    } else {
        printf("Firmware : (no app description)\n");
    }

    printf("Uptime   : %lld s\n", (long long)(esp_timer_get_time() / 1000000));

    // Useful after an OTA: a reset reason of "power-on" following an update
    // means the new image never actually ran.
    printf("Reset    : %s\n", resetReasonName(esp_reset_reason()));
    return 0;
}

}  // namespace

bool install() {
    static const console::Command COMMANDS[] = {
        {"reboot",
         "Restart the device",
         nullptr,
         handleReboot},

        {"version",
         "Show firmware version, build time and reset reason",
         nullptr,
         handleVersion},
    };

    return console::addFeature("system", COMMANDS, std::size(COMMANDS));
}

}  // namespace system_commands
