Architecture
============

System Overview
---------------

The ESP32 Smart Home firmware uses a dual-core FreeRTOS architecture with event-driven design.

Core Allocation
---------------

Core 0: Networking Stack
~~~~~~~~~~~~~~~~~~~~~~~~~~

* **WifiService** (Priority 5) - WiFi STA connection management
* **MqttService** (Priority 5) - Async MQTT client
* **OTAService** (Priority 6) - HTTPS OTA updates
* **UartDriver** (Priority 4) - UART RX interrupt handling

Core 1: Application Logic
~~~~~~~~~~~~~~~~~~~~~~~~~~

* **AppStateMachine** (Priority 4) - Main state coordinator
* **AudioStateMachine** (Priority 4) - Audio FSM
* **AudioPipeline** (Priority 5) - I2S DMA audio capture
* **WakeWordService** (Priority 3) - ESP-SR wake word detection
* **OledDisplay** (Priority 2) - SSD1306 display driver
* **PowerManager** (Priority 3) - Power mode management
* **WatchdogSupervisor** (Priority 2) - Task monitoring

Inter-Process Communication
-----------------------------

EventBus
~~~~~~~~

The EventBus is the central communication mechanism:

* **Thread-safe**: Uses FreeRTOS queues and mutexes
* **Publish/Subscribe**: Decoupled service communication
* **Type-safe**: Strongly-typed event messages
* **Non-blocking**: Queue-based message passing

Event Types
~~~~~~~~~~~

* WiFi Events: WIFI_STARTED, WIFI_CONNECTED, WIFI_DISCONNECTED, WIFI_GOT_IP, WIFI_ERROR
* MQTT Events: MQTT_CONNECTED, MQTT_DISCONNECTED, MQTT_DATA_RECEIVED, MQTT_ERROR
* Audio Events: AUDIO_STARTED, AUDIO_STOPPED, AUDIO_ERROR
* OTA Events: OTA_STARTED, OTA_PROGRESS, OTA_COMPLETED, OTA_FAILED
* Power Events: POWER_MODE_CHANGED, WAKE_UP
* System Events: WAKE_WORD_DETECTED, STATE_CHANGED, SYSTEM_ERROR

State Machines
--------------

Application State Machine
~~~~~~~~~~~~~~~~~~~~~~~~~

States:
* INIT
* WIFI_CONNECTING
* WIFI_CONNECTED
* MQTT_CONNECTING
* RUNNING
* OTA_UPDATING
* ERROR
* SLEEP

Audio State Machine
~~~~~~~~~~~~~~~~~~~

States:
* AUDIO_INIT
* AUDIO_IDLE
* AUDIO_LISTENING
* AUDIO_WAKE_DETECTED
* AUDIO_PROCESSING
* AUDIO_SUSPENDED

Power Management
----------------

Power Modes
~~~~~~~~~~~

* **NORMAL**: All services active
* **MODEM_SLEEP**: WiFi modem sleep, CPU active
* **LIGHT_SLEEP**: CPU and peripherals sleep

Wake-up Sources
~~~~~~~~~~~~~~~

* Voice (AudioPipeline wake word detection)
* UART interrupt
* MQTT command
* Timer

Sleep Conditions
~~~~~~~~~~~~~~~~

System enters LIGHT_SLEEP when:
* No WiFi activity
* No MQTT traffic
* Audio in IDLE state
* No OTA in progress

Audio Pipeline
--------------

I2S Configuration
~~~~~~~~~~~~~~~~~

* Sample Rate: 16 kHz (configurable)
* Bits per Sample: 16-bit (configurable)
* Channels: Mono
* DMA Buffers: 8 buffers × 1024 bytes
* Ring Buffer: 8192 bytes

Data Flow
~~~~~~~~~

1. I2S peripheral captures audio via DMA
2. AudioPipeline task reads from DMA buffers
3. Data written to ring buffer (zero-copy where possible)
4. WakeWordService reads from ring buffer
5. ESP-SR WakeNet processes audio for wake word detection

Task Watchdog
-------------

Monitored Tasks
~~~~~~~~~~~~~~~

* WifiService (30s timeout)
* MqttService (30s timeout)
* AppStateMachine (30s timeout)
* AudioPipeline (30s timeout)
* WakeWordService (30s timeout)

Monitoring Strategy
~~~~~~~~~~~~~~~~~~~

1. **ESP Task WDT**: Hardware watchdog, tasks feed via ``esp_task_wdt_reset()``
2. **Heartbeat EventGroup**: Software monitoring, tasks set bits periodically
3. **WatchdogSupervisor**: Monitors heartbeats, logs stalled tasks, triggers safe reset

Error Handling
--------------

Error Categories
~~~~~~~~~~~~~~~~

* WIFI_ERROR
* MQTT_ERROR
* DISPLAY_ERROR
* AUDIO_ERROR
* SYSTEM_ERROR
* NVS_ERROR
* POWER_ERROR
* OTA_ERROR
* UART_ERROR

Error Propagation
~~~~~~~~~~~~~~~~~

1. Service detects error
2. Calls ``ErrorHandler::reportError()``
3. ErrorHandler publishes SYSTEM_ERROR event
4. AppStateMachine receives event
5. Transitions to ERROR state (if critical)

Home Assistant Integration
---------------------------

Auto-Discovery
~~~~~~~~~~~~~~

MQTT topic: ``homeassistant/binary_sensor/esp32_smart_home/config``

Topics
~~~~~~

* State: ``smart_home/esp32_smart_home/state``
* Availability: ``smart_home/esp32_smart_home/availability``
* Wake Word: ``smart_home/wake_word``
* OTA Trigger: ``smart_home/ota/trigger``

OTA Updates
-----------

Process
~~~~~~~

1. Trigger via MQTT: ``smart_home/ota/trigger`` with HTTPS URL
2. Download firmware via ``esp_https_ota``
3. Verify signature (if configured)
4. Write to OTA partition
5. Publish progress via MQTT (0-100%)
6. Auto-restart on completion

Safety Features
~~~~~~~~~~~~~~~

* Rollback protection (ESP-IDF automatic rollback)
* Sleep blocking during OTA
* Progress reporting
* Error handling

