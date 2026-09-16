ESP32-S3 Smart Home
===================

A DIY smart home whose devices are controllable from Google Home, built from an
ESP32-S3 node and a Raspberry Pi acting as the hub.

.. toctree::
   :maxdepth: 2
   :caption: Concept

   architecture

.. toctree::
   :maxdepth: 2
   :caption: Using it

   getting-started
   deployment

.. toctree::
   :maxdepth: 2
   :caption: Working on it

   api/index
   development

.. note::

   - `Doxygen API reference <doxygen/index.html>`_
   - `GitHub repository <https://github.com/sprchuoi/smart_home_idf>`_
   - `Report an issue <https://github.com/sprchuoi/smart_home_idf/issues>`_

The idea
--------

The obvious way to get a DIY device into Google Home is to run Matter on the
microcontroller. That works, but it is the hard road, and it turns out to be
unnecessary.

**Google Home only ever needs to see a Matter bridge.** How that bridge gets
its data is irrelevant to Google. So the microcontroller can speak plain MQTT,
and the Raspberry Pi can do the bridging:

.. code-block:: text

        Google Home app  ·  Nest speaker
                    │  Matter (local only, QR pairing)
        ┌───────────▼─────────────────────────────┐
        │  Raspberry Pi                            │
        │    Mosquitto   ← MQTT broker             │
        │    Home Assistant                        │
        │      └─ Matter bridge add-on             │
        └───────────┬──────────────────────────────┘
                    │  MQTT over WiFi 2.4 GHz
        ┌───────────▼──────────────────────────────┐
        │  ESP32-S3 nodes — sensors + actuators    │
        └──────────────────────────────────────────┘

What that buys:

* **No recurring cost.** Test Matter vendor IDs are free. There is no cloud
  project, no OAuth server, and no public HTTPS endpoint to host.
* **Nothing exposed to the internet.** Matter is local, and so is MQTT.
* **No Matter stack on the microcontroller.** The node stays a small MQTT
  client, which leaves its flash and RAM for the actual job.
* **It keeps working when the internet does not.** Control is local end to end.

Status
------

This project is being built in phases. Being explicit about what is real
matters, because an earlier version of this documentation described services
that had been deleted.

.. list-table::
   :header-rows: 1
   :widths: 12 30 58

   * - Phase
     - Scope
     - State
   * - 1
     - Target ESP32-S3, dual-slot OTA, clear dead weight
     - Complete in firmware. Not yet run on hardware.
   * - 2
     - MQTT, Home Assistant discovery, NVS provisioning
     - Complete in firmware. No broker has been contacted yet.
   * - 3
     - Sensors
     - Not started
   * - 4
     - Google Home via the Matter bridge
     - Not started
   * - 5
     - Actuators
     - Not started
   * - 6
     - nRF5340 Thread sensor node
     - Not started
   * - 7
     - OTA hardening
     - Not started

``ROADMAP.md`` in the repository root tracks this in detail, including the
verification checklist that has to pass before Phase 1 can be called done.

What it runs on
---------------

.. list-table::
   :header-rows: 1
   :widths: 28 72

   * - Component
     - Notes
   * - ESP32-S3-DevKitC-1 (N16R8)
     - 16 MB flash, 8 MB octal PSRAM. Sensor and actuator nodes.
   * - Raspberry Pi
     - Mosquitto, Home Assistant, and the Matter bridge. The hub.
   * - nRF5340 DK
     - Reserved for a battery-powered Thread sensor node, later.
   * - ESP-IDF v5.5.1
     - The firmware toolchain.

Quick start
-----------

.. code-block:: bash

   ./make.sh setup          # check toolchain, install Python deps
   ./make.sh build          # build for ESP32-S3
   ./make.sh smoke          # verify the image before flashing it
   ./make.sh flash-monitor  # flash and open the serial console

Then provision the node over that console:

.. code-block:: text

   esp32> wifi_set <ssid> <password>
   esp32> mqtt_set <broker-host> [port]
   esp32> mqtt_device <device_id> <name> [room]
   esp32> reboot

Credentials live in NVS, not in the firmware image. See
:doc:`getting-started` for the full walkthrough.

Indices and tables
==================

* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`
