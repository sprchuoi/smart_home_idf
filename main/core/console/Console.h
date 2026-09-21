/**
 * @file Console.h
 * @brief Console command registration
 *
 * Features describe their commands in a static table and register the whole
 * group in one call. Before this, every command was eight lines of
 * near-identical struct-plus-ESP_ERROR_CHECK boilerplate repeated at each call
 * site, which is both tedious to extend and easy to get subtly wrong.
 *
 * Commands keep flat names (`wifi_set`, `mqtt_status`). That is deliberate:
 * esp_console's tab completion works per registered command, so subcommands
 * under a shared verb would complete only as far as the feature name, and
 * every existing reference in the docs and tools/ would have to change for a
 * cosmetic gain.
 *
 * Usage:
 *
 * @code
 *   static int onStatus(int argc, char** argv) { ... }
 *
 *   static const console::Command WIFI[] = {
 *       {"wifi_set",    "Set SSID and password\nUsage: wifi_set <ssid> <pass>",
 *                       "wifi_set <ssid> <pass>", onSetBoth},
 *       {"wifi_status", "Show WiFi configuration", nullptr, onStatus},
 *   };
 *
 *   console::addFeature("wifi", WIFI, std::size(WIFI));
 * @endcode
 */

#pragma once

#include <cstddef>

namespace console {

/// Command entry point. Return 0 on success, non-zero on failure.
using Handler = int (*)(int argc, char** argv);

/**
 * @brief One console command.
 *
 * `help` may contain a newline; the second and later lines are shown as-is, so
 * "Summary\nUsage: foo <bar>" reads well in the listing.
 *
 * `hint` is the usage shown by esp_console while the command is being typed.
 * It may be nullptr.
 *
 * Tables of these are held by pointer and must have static storage duration --
 * use a file-scope `static const` array, not a local.
 */
struct Command {
    const char* name;
    const char* help;
    const char* hint;
    Handler     handler;
};

/// How many features can be registered. Not a limit worth exceeding.
constexpr size_t MAX_FEATURES = 16;

/**
 * @brief Register a group of commands under a feature name.
 *
 * The feature name is only used for the grouped listing; it does not prefix
 * the command names.
 *
 * @return true if every command registered. On failure the ones that succeeded
 *         stay registered, and the offending name is logged.
 */
bool addFeature(const char* feature, const Command* commands, size_t count);

/**
 * @brief Print every registered command, grouped by feature.
 */
void listAll();

/**
 * @brief Replace the stock `help` with the grouped listing.
 *
 * esp_console installs its own `help`, which lists commands alphabetically with
 * no notion of which feature they belong to. This deregisters it and installs
 * one that groups by feature.
 *
 * Call once, after the console REPL is running and after every addFeature().
 */
bool install();

/**
 * @brief Usage check for handlers that take a fixed number of arguments.
 *
 * Replaces the same four lines repeated in every handler:
 *
 * @code
 *   if (!console::requireArgs(argc, 3, "mqtt_auth <username> <password>")) {
 *       return 1;
 *   }
 * @endcode
 *
 * @param argc   Handler's argc, including the command name.
 * @param min    Minimum argc, including the command name.
 * @param usage  Usage line to print when the check fails.
 * @return true when argc >= min; false after printing the usage.
 */
bool requireArgs(int argc, int min, const char* usage);

}  // namespace console
