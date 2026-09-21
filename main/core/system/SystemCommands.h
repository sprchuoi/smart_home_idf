/**
 * @file SystemCommands.h
 * @brief Device-level console commands
 *
 * Grouped under the "system" feature in `help`.
 */

#pragma once

namespace system_commands {

/**
 * @brief Register the system command group.
 *
 * Currently:
 *   reboot   -- restart the device
 *   version  -- firmware, build and reset-reason information
 *
 * Must be called before console::install(), so the grouped help includes it.
 */
bool install();

}  // namespace system_commands
