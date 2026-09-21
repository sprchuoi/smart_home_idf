Getting Started
===============

Build, flash and provision an ESP32-S3 node.

Prerequisites
-------------

Required:

* **ESP-IDF v5.5.1** -- the version this project is built and tested against
* **Python 3.8+**, **CMake 3.16+**, **Ninja**, **Git**

Optional:

* **Doxygen** and **Sphinx** -- to build these docs (``./make.sh doc``)
* **cppcheck** -- static analysis (``./make.sh test``)

Install ESP-IDF
---------------

.. code-block:: bash

   mkdir -p ~/esp && cd ~/esp
   git clone --recursive https://github.com/espressif/esp-idf.git
   cd esp-idf
   ./install.sh esp32s3
   . ./export.sh

Note the target: **esp32s3**, not ``esp32``.

Get the source
--------------

.. code-block:: bash

   git clone https://github.com/sprchuoi/smart_home_idf.git
   cd smart_home_idf
   ./make.sh setup

Build
-----

.. code-block:: bash

   ./make.sh build

The build target, flash size and PSRAM mode come from ``sdkconfig.defaults``,
which is committed. Do not set them by hand -- in particular, the N16R8 uses
**octal** PSRAM, and configuring the wrong mode produces a boot loop rather
than a build error.

Check the image before flashing
-------------------------------

.. code-block:: bash

   ./make.sh smoke

This verifies the things that actually go wrong: that the image targets
esp32s3, that flash is 16 MB, that PSRAM is octal, that logging is compiled in,
that ``otadata``/``ota_0``/``ota_1`` exist, and that the image fits a slot. It
fails loudly rather than leaving you to discover the problem on the bench.

Flash
-----

.. code-block:: bash

   ./make.sh flash-monitor

Provision
---------

Nothing device-specific is compiled in. Everything is set over the serial
console and stored in NVS.

**1. WiFi**

.. code-block:: text

   esp32> wifi_set <ssid> <password>

**2. MQTT broker**

.. code-block:: text

   esp32> mqtt_set <broker-host> [port]     # port defaults to 1883
   esp32> mqtt_auth <username> <password>   # omit for an anonymous broker

**3. Device identity**

.. code-block:: text

   esp32> mqtt_device <device_id> <name> [room]

``device_id`` must be lowercase letters, digits, ``-`` or ``_``. It becomes an
MQTT topic segment and a Home Assistant identifier, so anything else is
rejected rather than silently producing discovery topics that never match.

If you skip this step, an id is derived from the factory MAC as
``shnode-xxxxxx`` and stored, so it stays stable across reboots.

**4. Apply and verify**

.. code-block:: text

   esp32> mqtt_status
   esp32> version
   esp32> reboot

``version`` reports the running firmware, when it was built and why the device
last reset -- worth checking after an OTA, since a reset reason of "power-on"
following an update means the new image never actually ran.

Run ``help`` at any point to list every command, grouped by feature.

After rebooting you should see it associate, get an IP, and connect to the
broker:

.. code-block:: text

   I (4210) Application: Got IP 192.168.1.42
   I (4480) MqttService: Connected to 192.168.1.10:1883

The node then publishes retained discovery topics, so it appears in Home
Assistant automatically under Settings → Devices.

Troubleshooting
---------------

**The board boot-loops immediately.** Almost always the wrong PSRAM mode.
Confirm ``CONFIG_SPIRAM_MODE_OCT=y`` in ``sdkconfig`` -- the N16R8 has octal
PSRAM, and the Kconfig default is quad.

**``idf.py: command not found``.** The ESP-IDF environment is not sourced in
this shell:

.. code-block:: bash

   . ~/esp/esp-idf/export.sh      # affects only the current shell

You do not need it for the usual work: ``./make.sh`` sources ESP-IDF itself, so
``./make.sh build`` and friends work from a shell that has never seen
``export.sh``. Reach for ``idf.py`` directly only for things the script does
not wrap, and source first when you do.

Note also that sourcing does not persist across terminals, and that ``zsh``
and ``bash`` keep separate environments -- a shell where ``idf.py`` works
yesterday may not be the shell you are in today.

**The board boots but never connects.** Check ``mqtt_status`` for a missing
broker host, and confirm the broker is reachable from the same subnet.

**Nothing appears in Home Assistant.** Verify the node is publishing by
subscribing from the Pi:

.. code-block:: bash

   mosquitto_sub -h localhost -t 'homeassistant/#' -v

**Build failures after pulling.** ``./make.sh clean && ./make.sh build``.

Next steps
----------

* :doc:`architecture` -- how the firmware is put together
* :doc:`api/index` -- component reference
* :doc:`deployment` -- running it for real
