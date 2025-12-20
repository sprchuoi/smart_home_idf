# ESP32 Smart Home Application

A production-ready, low-power, real-time, voice-activated ESP32 smart home firmware using ESP-IDF v5.x, FreeRTOS, and modern C++ architecture.

[![CI/CD Pipeline](https://github.com/sprchuoi/smart_home_idf/actions/workflows/ci.yml/badge.svg)](https://github.com/sprchuoi/smart_home_idf/actions/workflows/ci.yml)
[![Documentation](https://github.com/sprchuoi/smart_home_idf/actions/workflows/docs.yml/badge.svg)](https://github.com/sprchuoi/smart_home_idf/actions/workflows/docs.yml)

📖 **[View Full Documentation](https://sprchuoi.github.io/smart_home_idf/)** | 📚 **[API Reference](https://sprchuoi.github.io/smart_home_idf/doxygen/)**

## Features

- **Dual-core FreeRTOS architecture** - Optimized task allocation (Core 0: Networking, Core 1: Application)
- **Audio DMA pipeline** - I2S audio capture with ESP-SR wake word detection
- **Power management** - Sleep modes (NORMAL, MODEM_SLEEP, LIGHT_SLEEP) with multiple wake-up sources
- **HTTPS OTA updates** - Secure over-the-air firmware updates with progress reporting
- **Home Assistant integration** - MQTT auto-discovery, QoS 0 communication
- **Task watchdog** - Real-time task monitoring with safe reset on timeout
- **Thread-safe IPC** - Event-driven architecture using FreeRTOS queues
- **UART communication** - Interrupt-driven UART RX handling

## Quick Start

```bash
# Setup environment
./make.sh setup

# Build project
./make.sh build

# Flash and monitor
./make.sh flash-monitor
```

## Prerequisites

- ESP-IDF v5.x
- Python 3.6+
- CMake 3.16+
- Ninja

## Configuration

Before building, configure:

1. **WiFi Credentials**: Use `WifiConfigService` API or NVS
2. **MQTT Broker**: Edit `main/app/Application.cpp`
3. **Hardware Pins**: Configure I2S, OLED, UART pins in `main/app/Application.cpp`

## Project Structure

```
smart_home/
├── main/              # Main application
│   ├── app/          # Application orchestrator
│   ├── core/         # Core services (EventBus, State Machines)
│   ├── services/      # Service layer (WiFi, MQTT, Audio, OTA)
│   ├── drivers/       # Hardware drivers (OLED, UART)
│   └── error/         # Error handling
├── docs/              # Sphinx documentation source
└── make.sh            # Build script
```

## Documentation

Full documentation is available at: **https://yourusername.github.io/smart_home/**

To build documentation locally:

```bash
./make.sh doc
```

Then open `docs/_build/html/index.html` in your browser.

## Build Commands

```bash
./make.sh setup          # Setup environment
./make.sh build          # Build project
./make.sh clean          # Clean build
./make.sh flash          # Flash to device
./make.sh monitor        # Monitor serial
./make.sh test           # Run tests
./make.sh test-qemu      # Run QEMU tests
./make.sh doc            # Generate documentation
./make.sh ci             # Run CI/CD pipeline
```

## Architecture

The system uses an event-driven architecture:

- **EventBus**: Central IPC system using FreeRTOS queues
- **State Machines**: Application and Audio state coordination
- **Services**: Decoupled services communicating via events
- **Drivers**: Hardware abstraction layer

See [Architecture Documentation](https://yourusername.github.io/smart_home/architecture.html) for details.

## License

This project is provided as-is for educational and professional use.

## Contributing

See [Development Guide](https://yourusername.github.io/smart_home/development.html) for contribution guidelines.
