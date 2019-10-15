.. _blinky-sample:

Blinky Application
##################

Overview
********

The Blinky example shows how to configure GPIO pins as outputs which can also be
used to drive LEDs on the hardware usually delivered as "User LEDs" on many of
the supported boards in Zephyr.

Requirements
************

The demo assumes that an LED is connected to one of GPIO lines. The
sample code is configured to work on boards with user defined buttons and that
have defined the LED0\_* variables in :file:`board.h`.

The :file:`board.h` must define the following variables:

- LED0_GPIO_NAME (or LED0_GPIO_CONTROLLER)
- LED0_GPIO_PIN


Building and Running
********************

This samples does not output anything to the console.  It can be built and
flashed to a board as follows:

.. zephyr-app-commands::
   :zephyr-app: samples/basic/blinky
   :board: arduino_101
   :goals: build flash
   :compact:

After flashing the image to the board, the user LED on the board should start to
blink.
