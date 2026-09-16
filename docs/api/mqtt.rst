MQTT
====

MqttService
-----------

The MQTT client. Publishes availability, status and per-channel telemetry,
announces entities to Home Assistant via MQTT discovery, and dispatches
commands to a registered callback.

Owns its own reconnect loop: esp-mqtt's built-in auto-reconnect is a fixed
10-second retry with no backoff, so it is disabled in favour of exponential
backoff with jitter.

.. doxygenclass:: MqttService
   :members:
   :undoc-members:

MqttConfigInterface
-------------------

Stores broker coordinates and device identity in NVS, and exposes the
``mqtt_*`` console commands.

.. doxygenclass:: MqttConfigInterface
   :members:
   :undoc-members:

Configuration
-------------

.. doxygenstruct:: MqttConfigInfo_st
   :members:
