Application State
=================

Tracks where the node is in its startup sequence. This is deliberately a state
*holder* rather than a state machine: it was previously a 294-line FSM with its
own FreeRTOS task and nine event subscriptions, none of which ever ran, because
it waited on events nothing published. What is actually useful is the state
itself, which the status topic reports.

.. doxygenclass:: AppStateMachine
   :members:
   :undoc-members:

States
------

.. doxygenenum:: AppState
