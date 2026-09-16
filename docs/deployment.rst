Deployment
==========

This covers the node side. The hub -- Mosquitto, Home Assistant and the Matter
bridge on the Raspberry Pi -- is described in :doc:`index`.

Building a release image
------------------------

.. code-block:: bash

   ./make.sh clean
   ./make.sh build
   ./make.sh smoke      # verify target, PSRAM, log level and OTA slots

The image lands at ``build/smart_home.bin``, with ``flash_args`` alongside it
listing every binary and offset. To flash the whole set in one go:

.. code-block:: bash

   esptool.py --chip esp32s3 write_flash @build/flash_args

WiFi and broker settings are **not** part of the image. They live in NVS and
are set per device over the console, so one firmware build serves every node
and no credentials are ever committed.

Tagging a release
-----------------

Pushing a ``v*`` tag runs the release job, which packages the firmware and
attaches it to a GitHub release. That URL is what the OTA command expects.

OTA updates
-----------

The partition table provides two 4 MB application slots. An update is written
to the inactive one and takes effect on the next reset.

Request an update over MQTT:

.. code-block:: bash

   mosquitto_pub -h <broker> \
     -t smart_home/<device_id>/cmd/ota \
     -m 'https://example.com/firmware/smart_home.bin'

If a default URL is stored (``mqtt_ota_url``), an empty payload reuses it:

.. code-block:: bash

   mosquitto_pub -h <broker> -t smart_home/<device_id>/cmd/ota -m ''

Progress is reported on the retained status topic. On success the node reboots
into the new slot.

.. warning::

   **Automatic rollback is not currently enabled.**
   ``CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`` is unset, so an image that
   downloads correctly but crashes at runtime will *not* be rolled back. Until
   that is enabled, treat OTA as requiring a physical recovery path.

Monitoring
----------

Serial
~~~~~~

.. code-block:: bash

   ./make.sh monitor

MQTT
~~~~

Everything the node publishes, for a given device id:

.. code-block:: bash

   mosquitto_sub -h <broker> -t 'smart_home/<device_id>/#' -v

Watch a command round-trip while triggering it from Home Assistant:

.. code-block:: bash

   mosquitto_sub -h <broker> -t 'smart_home/<device_id>/cmd/#' -v

Home Assistant discovery, to confirm the node is announcing itself:

.. code-block:: bash

   mosquitto_sub -h <broker> -t 'homeassistant/#' -v

The topics and their QoS levels are tabulated in :doc:`architecture`.

Replacing a node
----------------

Because identity is in NVS rather than the image, a replacement board keeps its
own identity. To move an existing identity to a new board, provision it with
the same ``device_id``:

.. code-block:: text

   esp32> mqtt_device <same-device-id> <same-name> <same-room>

Home Assistant will treat it as the same device, and any dashboards or
automations pointing at it keep working. Note that the new board will replay
the retained state topics it owns.

Removing a node
---------------

Erase its retained topics on the broker, or they will keep reappearing in Home
Assistant as unavailable entities:

.. code-block:: bash

   for t in availability status rssi/state heap/state uptime/state; do
     mosquitto_pub -h <broker> -r -n -t "smart_home/<device_id>/$t"
   done
   for e in rssi heap uptime; do
     mosquitto_pub -h <broker> -r -n -t "homeassistant/sensor/<device_id>/$e/config"
   done
   mosquitto_pub -h <broker> -r -n -t "homeassistant/binary_sensor/<device_id>/link/config"
