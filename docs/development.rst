Development Guide
=================

Building
--------

See :doc:`getting-started`.

Code structure
--------------

.. code-block:: text

   main/
   ├── app/            Application orchestrator -- owns services, wires callbacks
   ├── core/           Application state
   ├── services/       WiFi, MQTT, OTA -- each with its own NVS config
   ├── drivers/        UART
   └── error/          Error logging and counters

The rule that keeps this tidy: **only ``Application`` knows about more than one
service.** It constructs them and wires their callbacks. Nothing else reaches
across a service boundary.

Adding a service
----------------

1. Create the class under ``main/services/<name>/``.
2. Add its ``.cpp`` to ``SRCS`` in ``main/CMakeLists.txt``, and any new
   component it needs to ``REQUIRES``. Prefer the specific ``esp_driver_*``
   component over the legacy ``driver`` umbrella.
3. Own an instance in ``Application`` and construct it in ``initialize()``.
4. If it needs to tell anyone something, expose a
   ``std::function`` callback setter and invoke it -- do **not** add a shared
   event bus. See :doc:`architecture` for why that was removed.
5. If it has device-specific configuration, put it in NVS behind a console
   command, following ``MqttConfigInterface`` as the template.

Adding a telemetry channel
--------------------------

1. Publish it from wherever the value is known:
   ``m_mqtt_service.publishState("<channel>", "<value>")``.
2. Add a matching entry to ``MqttService::publishDiscovery()`` so Home
   Assistant knows about it, with the right ``device_class``, unit and
   ``state_class``.
3. Keep ``channel`` lowercase and free of slashes -- it becomes a topic
   segment.

Adding a command
----------------

Extend ``Application::onMqttCommand()``. It runs in the esp-mqtt task, so it
must not block: set a flag and let a service task do the work, as the ``ota``
command does.

Testing
-------

.. code-block:: bash

   ./make.sh smoke    # verify the built image: target, PSRAM, log level, OTA slots
   ./make.sh test     # static analysis

``smoke`` is the one that matters before flashing. It checks the things that
otherwise only show up on the bench -- wrong target, wrong PSRAM mode, logging
compiled out, missing OTA partitions, image too large.

There is no QEMU target. ``qemu-system-xtensa`` cannot emulate an ESP32-S3,
which is why the old QEMU job could never have passed and has been removed.

.. note::

   Host-side unit tests are still outstanding. Pure logic -- discovery payload
   construction, topic formatting, sensor value conversion -- should be tested
   on the host via ESP-IDF's ``linux`` preview target, which runs in CI in
   seconds with no hardware.

Code style
----------

* ``esp_log`` for logging; the level is set in ``sdkconfig.defaults``.
* RAII for resources. Prefer ``std::atomic`` / ``SemaphoreHandle_t`` over
  unsynchronised flags.
* No globals. Singletons are reserved for the NVS config interfaces, and only
  because an ``esp_console`` command handler is a bare function pointer with
  nowhere else to reach the store.
* Pin tasks explicitly with ``xTaskCreatePinnedToCore`` and state the core in a
  named constant.
* Comment *why*, not what. The interesting comments in this codebase are the
  ones explaining a non-obvious constraint -- for example why telemetry is QoS 0
  while availability is QoS 1.
