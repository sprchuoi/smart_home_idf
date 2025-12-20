Getting Started
=============

This guide will help you set up and build the ESP32 Smart Home firmware.

Prerequisites
-------------

Required
~~~~~~~~

* **ESP-IDF v5.x** - Espressif IoT Development Framework
* **Python 3.6+** - For build tools
* **CMake 3.16+** - Build system
* **Ninja** - Build backend
* **Git** - Version control

Optional
~~~~~~~~

* **QEMU** - For emulation testing
* **Doxygen** - For API documentation
* **cppcheck** - For static analysis

Installation
------------

1. Install ESP-IDF
~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   mkdir -p ~/esp
   cd ~/esp
   git clone --recursive https://github.com/espressif/esp-idf.git
   cd esp-idf
   ./install.sh esp32
   . ./export.sh

2. Clone Repository
~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   git clone https://github.com/yourusername/smart_home.git
   cd smart_home

3. Setup Environment
~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   ./make.sh setup

This will:
* Check prerequisites
* Verify ESP-IDF installation
* Install Python dependencies
* Create necessary directories

Configuration
-------------

WiFi Credentials
~~~~~~~~~~~~~~~~

Configure WiFi credentials using NVS:

.. code-block:: cpp

   WifiConfigService::getInstance().setSSID("YourSSID");
   WifiConfigService::getInstance().setPassword("YourPassword");

Or edit ``main/app/Application.cpp`` to set credentials programmatically.

MQTT Broker
~~~~~~~~~~~

Edit ``main/app/Application.cpp``:

.. code-block:: cpp

   const char* mqtt_broker = "mqtt://192.168.1.100:1883";
   const char* mqtt_client_id = "esp32_smart_home";

Hardware Configuration
~~~~~~~~~~~~~~~~~~~~~~

I2S Audio Pins (default):
* BCLK: GPIO 4
* WS: GPIO 5
* DIN: GPIO 18

OLED Display Pins (default):
* SDA: GPIO 21
* SCL: GPIO 22
* I2C Address: 0x3C

Building
--------

Standard Build
~~~~~~~~~~~~~~

.. code-block:: bash

   ./make.sh build

This will:
* Source ESP-IDF environment
* Configure project
* Build firmware
* Show build information

Clean Build
~~~~~~~~~~~

.. code-block:: bash

   ./make.sh clean
   ./make.sh build

Flashing
--------

Flash to Device
~~~~~~~~~~~~~~~

.. code-block:: bash

   ./make.sh flash

Flash and Monitor
~~~~~~~~~~~~~~~~~

.. code-block:: bash

   ./make.sh flash-monitor

Testing
-------

Run Tests
~~~~~~~~~

.. code-block:: bash

   ./make.sh test

QEMU Testing
~~~~~~~~~~~~

.. code-block:: bash

   ./make.sh setup-qemu
   ./make.sh test-qemu

Troubleshooting
---------------

ESP-IDF Not Found
~~~~~~~~~~~~~~~~~~

.. code-block:: bash

   export IDF_PATH=~/esp/esp-idf
   ./make.sh setup

Build Failures
~~~~~~~~~~~~~~

.. code-block:: bash

   ./make.sh clean
   ./make.sh build

Permission Denied
~~~~~~~~~~~~~~~~

.. code-block:: bash

   chmod +x make.sh

Next Steps
----------

* Read the :doc:`architecture` documentation
* Explore the :doc:`api/index` reference
* Check :doc:`development` guide

