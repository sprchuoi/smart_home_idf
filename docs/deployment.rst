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

Progress is reported on the response topic. On success the node reboots into
the new slot.

.. note::

   **HTTPS requires a correct system clock.** The server is verified against
   the CA bundle compiled into the image, and certificate validity is checked
   against the device's clock. A board with no RTC starts at 1970, at which
   point every certificate looks not-yet-valid and the handshake fails. Until
   time is synchronised (SNTP), HTTPS OTA will not work even though the
   configuration is correct -- this is a known gap, not a misconfiguration.

   Plain HTTP over a bench network sidesteps it, which is what the test
   procedure below is for.

.. warning::

   **Automatic rollback is not currently enabled.**
   ``CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`` is unset, so an image that
   downloads correctly but crashes at runtime will *not* be rolled back. Until
   that is enabled, treat OTA as requiring a physical recovery path.

Testing OTA on the bench
------------------------

You can exercise the whole path with a local HTTP server and no SD card, no
cloud and no HTTPS.

**1. Build a firmware that permits plain HTTP.**

``esp_https_ota`` refuses ``http://`` URLs by default. The escape hatch is a
Kconfig option the firmware deliberately does *not* carry in its normal
configuration, because ESP-IDF's own help warns it means "accepting firmware
upgrade image from server with fake identity" -- anyone on the network path
could substitute what your device installs. It lives in a separate file, and
this command is the only thing that applies it:

.. code-block:: bash

   ./make.sh build-ota-test
   ./make.sh flash-monitor

The command regenerates ``sdkconfig`` first, because ``SDKCONFIG_DEFAULTS`` is
only consulted when that file is created -- without it an existing
``sdkconfig`` keeps its old values and the build silently comes out unchanged.
It also prints whether plain-HTTP OTA ended up enabled, so the image's
configuration is never a guess.

**2. Make the version change.**

The check that matters is whether ``firmware_version`` changes after the
update, and that string comes from the git revision. An image identical to the
one already running will update "successfully" and prove nothing. Move HEAD
before building the *second* image:

.. code-block:: bash

   git commit --allow-empty -m "ota test: v2"
   ./make.sh build-ota-test

**3. Serve it.**

.. code-block:: bash

   tools/ota/serve.py --port 8070

It prints the URL to use and logs every request the device makes, which is most
of the diagnostic value when a transfer fails. Use the LAN address, not
localhost: this server has to be reachable *from the device*.

**4. Run the test.**

.. code-block:: bash

   tools/ota/test-ota.sh shnode-01 http://<your-lan-ip>:8070/smart_home.bin

It records the current version, triggers the update over MQTT, streams progress,
then waits for the device to reboot and reconnect. It passes only if the
version actually changed -- a device that merely reconnects is not evidence.

**5. Return to a production configuration.**

.. code-block:: bash

   ./make.sh build-production

which drops ``ESP_HTTPS_OTA_ALLOW_HTTP`` and puts you back on HTTPS-only. It
reports which state it ended in, so there is no ambiguity about what is
currently flashed.

.. note::

   The server implemented by ``tools/ota/serve.py`` supports HTTP Range
   requests, which is not incidental: ``python -m http.server`` does not, and
   ``partial_http_download`` requires it. Partial download is disabled in the
   firmware for that reason, so a plain static server works too -- but if you
   re-enable it, you need a server like this one.

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
