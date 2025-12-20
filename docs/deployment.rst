Deployment
==========

Production Build
----------------

.. code-block:: bash

   ./make.sh clean
   ./make.sh build

Configuration
-------------

Before deployment:

1. Set WiFi credentials
2. Configure MQTT broker
3. Set OTA server URL
4. Configure hardware pins

OTA Updates
-----------

The firmware supports HTTPS OTA updates:

1. Build firmware
2. Host on HTTPS server
3. Trigger via MQTT: ``smart_home/ota/trigger``
4. Monitor progress via MQTT

Monitoring
----------

Serial Monitor
~~~~~~~~~~~~~~

.. code-block:: bash

   ./make.sh monitor

MQTT Monitoring
~~~~~~~~~~~~~~~

Subscribe to topics:
* ``smart_home/esp32_smart_home/state``
* ``smart_home/esp32_smart_home/availability``
* ``smart_home/wake_word``

Troubleshooting
--------------

See :doc:`getting-started` for troubleshooting tips.

