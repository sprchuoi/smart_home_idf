WiFi
====

WifiService
-----------

Station connection management with auto-reconnect. Reports link-state changes
through a registered callback rather than the removed EventBus.

.. doxygenclass:: WifiService
   :members:
   :undoc-members:

WifiConfigInterface
-------------------

Stores WiFi credentials in NVS and exposes the ``wifi_*`` console commands.

.. doxygenclass:: WifiConfigInterface
   :members:
   :undoc-members:
