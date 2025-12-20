# WiFi Configuration Guide

## Overview

The `WifiConfigService` and `WifiProvisioning` services provide a robust way to configure WiFi credentials on your ESP32 Smart Home device. Credentials are stored in NVS (Non-Volatile Storage) and persist across reboots.

## Features

- **Persistent Storage**: WiFi credentials stored in NVS
- **Console Commands**: Configure WiFi via serial console
- **Programmatic API**: Set credentials in code
- **Thread-Safe**: Concurrent access protected by mutexes
- **Status Checking**: Query current configuration state

## Usage Methods

### Method 1: Console Commands (Recommended)

After flashing your device, use the serial monitor to configure WiFi:

```bash
# Monitor the device
./make.sh monitor

# Set both SSID and password at once
wifi_set MyNetworkName MyPassword123

# Or set them separately
wifi_ssid MyNetworkName
wifi_password MyPassword123

# Check current configuration
wifi_status

# Clear credentials
wifi_clear
```

After setting credentials, restart the device:
```bash
# Press Ctrl+] to exit monitor, then:
./make.sh flash monitor
```

### Method 2: Programmatic Configuration

Edit `main/app/src/Application.cpp` and uncomment/modify this line:

```cpp
// Optional: Set default credentials here for testing
WifiProvisioning::getInstance().setDefaultCredentials("YourSSID", "YourPassword");
```

This will set credentials only if none exist in NVS.

### Method 3: Direct API

You can also configure WiFi programmatically anywhere in your code:

```cpp
#include "services/wifi/WifiProvisioning.h"

// Set credentials
WifiProvisioning::getInstance().setCredentials("MySSID", "MyPassword");

// Check if configured
if (WifiProvisioning::getInstance().hasCredentials()) {
    // Credentials exist
}

// Print status
WifiProvisioning::getInstance().printStatus();
```

## Console Commands Reference

| Command | Arguments | Description |
|---------|-----------|-------------|
| `wifi_set` | `<ssid> <password>` | Set both SSID and password |
| `wifi_ssid` | `<ssid>` | Set SSID only |
| `wifi_password` | `<password>` | Set password only |
| `wifi_status` | - | Show current configuration |
| `wifi_clear` | - | Clear all credentials |

## Examples

### First Time Setup

```bash
# 1. Flash the device
./make.sh flash monitor

# 2. Wait for boot messages, then configure WiFi
wifi_set "My Home Network" "SuperSecretPassword"

# 3. Restart to connect
# Press Ctrl+] then run:
./make.sh monitor
```

### Changing WiFi Network

```bash
# In the serial monitor:
wifi_set "New Network" "NewPassword"

# Restart device
# Press Ctrl+] and reflash or press reset button
```

### Checking Configuration

```bash
# In the serial monitor:
wifi_status

# Output example:
# WiFi SSID: MyNetwork
# Password: [CONFIGURED]
# Status: Ready to connect
```

## Troubleshooting

### WiFi credentials not configured error

If you see this error:
```
E (1234) WifiService: WiFi credentials not configured!
E (1234) WifiService: Use console command: wifi_set <ssid> <password>
```

**Solution**: Use the `wifi_set` command to configure your WiFi credentials.

### Can't see console commands

Make sure you're using the ESP-IDF console. The commands are registered during initialization.

### Credentials not persisting

1. Check if NVS is properly initialized (should see "NVS initialized" in logs)
2. Verify NVS partition exists in partition table
3. Try clearing NVS: `nvs_flash_erase()` (requires reflash)

### Connection still failing after setting credentials

1. Verify credentials with `wifi_status`
2. Check WiFi network is 2.4GHz (ESP32 doesn't support 5GHz)
3. Check logs for specific connection errors
4. Try clearing and resetting credentials

## Architecture

### Components

1. **WifiConfigService**: Low-level NVS storage/retrieval
2. **WifiProvisioning**: High-level provisioning interface and console commands
3. **WifiService**: Uses credentials to establish WiFi connection

### Data Flow

```
User Input → WifiProvisioning → WifiConfigService → NVS Storage
                                        ↓
                                 WifiService → WiFi Connection
```

## Security Notes

- ⚠️ **Passwords are stored in plain text** in NVS (encrypted NVS available in ESP-IDF)
- ✅ Passwords are never displayed in `wifi_status` output
- ✅ Console commands are only accessible via physical serial connection
- 💡 Consider implementing WiFi Protected Setup (WPS) or BLE provisioning for production

## Advanced Usage

### Enable NVS Encryption (Recommended for Production)

1. Enable in menuconfig:
```bash
idf.py menuconfig
# Navigate to: Component config → NVS → Enable NVS encryption
```

2. Flash encryption keys:
```bash
esptool.py --port /dev/ttyUSB0 burn_efuse_key
```

### Custom Console Integration

You can integrate WiFi provisioning into your own console system:

```cpp
// Register during initialization
WifiProvisioning::getInstance().initialize();

// Commands are automatically registered with esp_console
```

## API Reference

See [WifiProvisioning.h](WifiProvisioning.h) and [WifiConfigService.h](WifiConfigService.h) for detailed API documentation.
