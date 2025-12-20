Development Guide
==================

Building
--------

See :doc:`getting-started` for build instructions.

Code Structure
--------------

.. code-block:: text

   main/
   ├── app/          # Application orchestrator
   ├── core/          # Core services (EventBus, State Machines)
   ├── services/       # Service layer (WiFi, MQTT, Audio, OTA)
   ├── drivers/       # Hardware drivers (OLED, UART)
   └── error/         # Error handling

Adding a New Service
--------------------

1. Create service class (``.h`` and ``.cpp``)
2. Add to ``main/CMakeLists.txt``
3. Initialize in ``Application::initialize()``
4. Subscribe/publish to EventBus
5. Register with watchdog (if critical)

Adding a New Event
-------------------

1. Add to ``EventType`` enum in ``EventBus.h``
2. Add payload structure to ``EventPayload`` union if needed
3. Publish from service
4. Subscribe in state machine/application

Testing
-------

Unit Tests
~~~~~~~~~~

Create tests in ``tests/`` directory:

.. code-block:: bash

   ./make.sh test

QEMU Testing
~~~~~~~~~~~~

.. code-block:: bash

   ./make.sh test-qemu

Static Analysis
~~~~~~~~~~~~~~~

.. code-block:: bash

   cppcheck --enable=all main/

Code Style
----------

* Use ``esp_log`` for logging
* Follow ESP-IDF coding standards
* Use RAII for resource management
* No global variables (except controlled singletons)
* Thread-safe design

