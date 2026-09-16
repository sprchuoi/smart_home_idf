Architecture
============

This page describes the firmware: how it is laid out, how its parts talk to
each other, and how it appears to Home Assistant.

The wider system -- why a Raspberry Pi does the Matter bridging, and why the
microcontroller does not -- is on :doc:`index`.

Firmware layout
---------------

.. code-block:: text

   main/
   ├── main.cpp                     app_main(): construct, initialize, run
   ├── app/src/Application.*        owns the services, wires their callbacks
   ├── core/statemachine/           application state holder
   ├── services/
   │   ├── wifi/                    station connection + NVS provisioning
   │   ├── mqtt/                    MQTT client + NVS provisioning
   │   └── ota/                     HTTPS A/B update
   ├── drivers/uart/                optional second UART
   └── error/ErrorHandler.*         logging + per-category error counts

``Application`` is the only place that knows about more than one service. It
constructs them, wires them together, and owns their lifetime. Nothing else
reaches across a service boundary.

How the parts talk
------------------

Services communicate through **direct callbacks**, not a publish/subscribe bus.

That is a deliberate change. An earlier revision had an ``EventBus`` that
queued fixed-size event structs through a FreeRTOS queue. It was removed for
three reasons:

1. **The topology is a DAG, not a bus.** The real flow is
   ``WiFi → MQTT → {publish, command → OTA}``. Nothing fans out, and every edge
   has exactly one listener, so a bus was pure overhead.
2. **The bus had no consumers.** Its only drain points were called from
   services that were never instantiated, so events were queued and dropped.
3. **It made a whole class of bug possible.** Because events were copied into a
   queue by value, any payload containing a pointer was copied as a *pointer*.
   ``AppStateMachine`` did exactly that with ``getStateString().c_str()``, which
   returns ``std::string`` by value -- so the pointer dangled before the event
   was even published.

With callbacks the same mistake is not expressible: nothing is queued, so
nothing can outlive its scope.

The edges are:

.. list-table::
   :header-rows: 1
   :widths: 30 30 40

   * - From
     - To
     - Trigger
   * - ``WifiService``
     - ``Application::onWifiEvent``
     - Station start, got IP, disconnected
   * - ``MqttService``
     - ``Application::onMqttCommand``
     - A message on ``smart_home/<id>/cmd/#``
   * - ``OTAService``
     - progress callback
     - Download progress percentage

Tasks and cores
---------------

.. list-table::
   :header-rows: 1
   :widths: 24 10 10 12 44

   * - Task
     - Core
     - Prio
     - Stack
     - Job
   * - ``WifiService``
     - 0
     - 5
     - 3072
     - Drains the WiFi event queue, drives reconnection
   * - ``MqttService``
     - 0
     - 5
     - 4096
     - Owns the MQTT reconnect loop
   * - ``OTAService``
     - 0
     - 6
     - 8192
     - Performs updates when requested
   * - esp-mqtt internal
     - 0
     - 5
     - 4096
     - Provided by the esp-mqtt component

Everything is pinned to Core 0. This is worth knowing because older
documentation described a "dual-core" split with application logic on Core 1 --
that split never existed in the code. Core 0 is also where the WiFi and lwIP
stacks live, which is the usual reason to keep networking tasks there. Core 1
currently runs only the idle task.

Committing to a real core split is deferred until there is work to put on
Core 1; guessing now would just create a claim the code does not honour.

MQTT interface
--------------

Topics are namespaced under ``smart_home/<device_id>/``.

.. list-table::
   :header-rows: 1
   :widths: 38 8 10 44

   * - Topic
     - QoS
     - Retained
     - Purpose
   * - ``smart_home/<id>/availability``
     - 1
     - Yes
     - ``online`` / ``offline``. Published on connect; the broker publishes
       ``offline`` via the Last Will if the node dies.
   * - ``smart_home/<id>/status``
     - 1
     - Yes
     - JSON: firmware, uptime, heap, RSSI, reset reason.
   * - ``smart_home/<id>/<channel>/state``
     - 0
     - No
     - One telemetry value per channel.
   * - ``smart_home/<id>/cmd/<target>``
     - 1
     - No
     - Inbound commands. Currently ``ota`` and ``reboot``.

QoS is chosen per class of traffic rather than fixed:

* **Availability, discovery and status use QoS 1 and are retained**, because
  all three have to survive a broker restart. A retained availability of
  ``offline`` is also what stops Home Assistant showing stale data forever.
* **Telemetry uses QoS 0 and is not retained.** A stale temperature reading has
  no value, and QoS 1 telemetry on a node whose broker is unreachable simply
  fills the outbox with unacknowledged publishes.
* **Commands use QoS 1**, and the session is persistent, so a command sent while
  the node is rebooting is queued by the broker rather than dropped.

Home Assistant discovery
------------------------

The node announces itself using MQTT discovery, publishing one retained config
topic per entity under ``homeassistant/<component>/<id>/<object>/config``. Every
entity carries a shared ``device`` block, so they group under a single device
rather than appearing as unrelated entries.

Discovery is republished on **every** connect, not just the first. That is how
changes to the name, room or software version propagate, and how the node
recovers if its retained topics are deleted.

The entities that exist today are diagnostics, chosen because they need no
sensor hardware -- which lets the whole pipeline be verified before any sensor
is wired up:

* ``Link`` -- connectivity, driven by the availability topic
* ``WiFi Signal`` -- RSSI
* ``Free Heap``
* ``Uptime``

Provisioning
------------

Nothing device-specific is compiled into the firmware. Broker coordinates,
WiFi credentials and device identity all live in NVS and are set over the serial
console:

.. code-block:: text

   esp32> wifi_set <ssid> <password>
   esp32> mqtt_set <broker-host> [port]
   esp32> mqtt_auth <username> <password>
   esp32> mqtt_device <device_id> <name> [room]
   esp32> mqtt_status
   esp32> reboot

Device identity is validated to ``[a-z0-9_-]``. It becomes both an MQTT topic
segment and a Home Assistant identifier, and a stray ``/`` or capital letter
produces discovery topics that silently never match.

If ``device_id`` is left unset it is derived from the factory MAC as
``shnode-xxxxxx`` and written back to NVS, so it cannot drift between boots.

OTA
---

Updates use HTTPS against ``esp_https_ota``'s incremental API and write to the
inactive slot; the bootloader swaps on the next reset. The partition table
provides two 4 MB slots, and ``idf.py build`` fails outright if the image does
not fit one.

An update is requested over MQTT:

.. code-block:: text

   mosquitto_pub -t smart_home/<id>/cmd/ota -m 'https://host/firmware.bin'

The command handler only sets a flag and returns; the OTA task performs the
transfer, so a slow download never blocks the MQTT client.

.. warning::

   Automatic rollback is **not** enabled. ``CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE``
   is unset, so an image that passes its checksum but fails at runtime will stay
   in place. Enabling and testing rollback is Phase 7 work.

What is deliberately absent
---------------------------

Recorded so their absence does not read as an oversight:

* **Audio capture and wake-word detection.** Removed. No working wake-word model
  existed anywhere in the project, and the audio pipeline did not compile for
  the ESP32-S3 -- it used an I2S slot field that only exists on the original
  ESP32. It can return if voice is ever wanted.
* **Power management.** Removed. Only ``NORMAL`` was implemented; modem sleep
  was a stub, and light sleep could sleep indefinitely on a zero-length timer.
  WiFi nodes are not battery powered, so it had no job here.
* **OLED display.** Removed. Its render path was a stub with no framebuffer or
  font, and it contained a call that deleted the calling task.
* **A custom task watchdog.** Removed in favour of IDF's own task watchdog,
  which is configured through ``CONFIG_ESP_TASK_WDT_TIMEOUT_S`` and actually
  works. The previous wrapper never programmed the hardware watchdog at all, and
  its "feed on behalf of another task" call reset the wrong subscription.
