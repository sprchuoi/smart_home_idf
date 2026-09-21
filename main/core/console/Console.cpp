/**
 * @file Console.cpp
 * @brief Console command registration
 */

#include "Console.h"

#include "esp_console.h"
#include "esp_log.h"

#include <cstdio>
#include <cstring>

namespace console {
namespace {

const char* TAG = "Console";

struct Feature {
    const char*     name;
    const Command*  commands;
    size_t          count;
};

Feature s_features[MAX_FEATURES];
size_t  s_feature_count = 0;

int handleHelp(int argc, char** argv);

}  // namespace

bool addFeature(const char* feature, const Command* commands, size_t count) {
    if (feature == nullptr || commands == nullptr || count == 0) {
        ESP_LOGE(TAG, "addFeature: nothing to register");
        return false;
    }
    if (s_feature_count >= MAX_FEATURES) {
        ESP_LOGE(TAG, "addFeature('%s'): at most %u features (raise MAX_FEATURES)",
                 feature, (unsigned)MAX_FEATURES);
        return false;
    }

    bool all_ok = true;

    for (size_t i = 0; i < count; ++i) {
        const Command& c = commands[i];
        if (c.name == nullptr || c.handler == nullptr) {
            ESP_LOGE(TAG, "addFeature('%s'): entry %u needs at least name and handler",
                     feature, (unsigned)i);
            all_ok = false;
            continue;
        }

        esp_console_cmd_t cmd = {};
        cmd.command = c.name;
        cmd.help    = c.help;
        cmd.hint    = c.hint;
        cmd.func    = c.handler;
        cmd.argtable = nullptr;

        // Logged rather than ESP_ERROR_CHECK'd: one bad command should not
        // abort the boot of a device that is otherwise fine.
        esp_err_t err = esp_console_cmd_register(&cmd);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "addFeature('%s'): '%s' failed to register: %s",
                     feature, c.name, esp_err_to_name(err));
            all_ok = false;
        }
    }

    // Recorded even on partial failure, so the listing reflects what is
    // actually available rather than what was intended.
    s_features[s_feature_count++] = {feature, commands, count};

    ESP_LOGI(TAG, "Registered %u command(s) for '%s'", (unsigned)count, feature);
    return all_ok;
}

void listAll() {
    size_t width = 0;
    for (size_t f = 0; f < s_feature_count; ++f) {
        for (size_t i = 0; i < s_features[f].count; ++i) {
            const char* n = s_features[f].commands[i].name;
            if (n != nullptr) {
                width = std::strlen(n) > width ? std::strlen(n) : width;
            }
        }
    }
    if (width == 0) {
        printf("\nNo commands registered.\n");
        return;
    }

    for (size_t f = 0; f < s_feature_count; ++f) {
        const Feature& feat = s_features[f];
        printf("\n%s\n", feat.name);

        for (size_t i = 0; i < feat.count; ++i) {
            const Command& c = feat.commands[i];
            const char* line = (c.help != nullptr) ? c.help : "";
            const char* nl = nullptr;
            bool first = true;

            // Every line is emitted, not just the first. A help string may
            // carry more than one newline -- mqtt_device has a Usage line and
            // a note about the id format -- and indenting only the first left
            // the rest hugging the left margin.
            do {
                nl = std::strchr(line, '\n');
                const int len = nl ? (int)(nl - line) : (int)std::strlen(line);

                // %-*s, not %-*.*s: a precision would silently truncate any
                // command name longer than the column instead of widening it.
                printf("  %-*s  %.*s\n", (int)width, first ? c.name : "", len, line);

                line = nl ? nl + 1 : nullptr;
                first = false;
            } while (line != nullptr);
        }
    }

    printf("\nUse TAB to complete a command name, UP/DOWN for history.\n");
}

bool requireArgs(int argc, int min, const char* usage) {
    if (argc >= min) {
        return true;
    }
    printf("Usage: %s\n", usage != nullptr ? usage : "<command>");
    return false;
}

bool install() {
    // esp_console installs a flat, alphabetical `help` when the REPL starts.
    // Deregistering first is required -- re-registering the same name is an
    // error, not a replacement.
    esp_err_t err = esp_console_deregister_help_command();
    if (err != ESP_OK && err != ESP_ERR_NOT_FOUND) {
        ESP_LOGW(TAG, "Could not remove the stock help command: %s",
                 esp_err_to_name(err));
    }

    esp_console_cmd_t help = {};
    help.command = "help";
    help.help    = "List commands, grouped by feature";
    help.hint    = nullptr;
    help.func    = handleHelp;
    help.argtable = nullptr;

    err = esp_console_cmd_register(&help);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install grouped help: %s", esp_err_to_name(err));
        // Put the stock one back so the console is not left without help.
        esp_console_register_help_command();
        return false;
    }

    ESP_LOGI(TAG, "Grouped help installed");
    return true;
}

namespace {

int handleHelp(int argc, char** argv) {
    // `help` with no argument lists everything, which is the common case.
    // A command name filters to just that entry, matching the stock behaviour
    // closely enough that muscle memory carries over.
    if (argc < 2) {
        listAll();
        return 0;
    }

    for (size_t f = 0; f < s_feature_count; ++f) {
        for (size_t i = 0; i < s_features[f].count; ++i) {
            const Command& c = s_features[f].commands[i];
            if (c.name != nullptr && std::strcmp(c.name, argv[1]) == 0) {
                printf("%s\n\n  %s\n\n", c.name, c.help != nullptr ? c.help : "");
                if (c.hint != nullptr) {
                    printf("  Usage: %s\n", c.hint);
                }
                return 0;
            }
        }
    }

    printf("No such command: %s\n", argv[1]);
    printf("Run 'help' for the list.\n");
    return 1;
}

}  // namespace

}  // namespace console
