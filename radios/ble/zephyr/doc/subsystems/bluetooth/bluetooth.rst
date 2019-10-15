.. _bluetooth:

Bluetooth
#########

Zephyr comes integrated with a feature-rich and highly configurable
Bluetooth stack:

* Bluetooth 5.0 compliant (ESR10)

* Bluetooth Low Energy Controller support (LE Link Layer)

  * BLE 5.0 compliant
  * Unlimited role and connection count, all roles supported
  * Concurrent multi-protocol support ready
  * Intelligent scheduling of roles to minimize overlap
  * Portable design to any open BLE radio, currently supports Nordic
    Semiconductor nRF51 and nRF52

* Generic Access Profile (GAP) with all possible LE roles

  * Peripheral & Central
  * Observer & Broadcaster

* GATT (Generic Attribute Profile)

  * Server (to be a sensor)
  * Client (to connect to sensors)

* Pairing support, including the Secure Connections feature from Bluetooth 4.2

* IPSP/6LoWPAN for IPv6 connectivity over Bluetooth LE

  * IPSP node sample application in ``samples/bluetooth/ipsp``

* Basic Bluetooth BR/EDR (Classic) support

  * Generic Access Profile (GAP)
  * Logical Link Control and Adaptation Protocol (L2CAP)
  * Serial Port emulation (RFCOMM protocol)
  * Service Discovery Protocol (SDP)

* Clean HCI driver abstraction

  * 3-Wire (H:5) & 5-Wire (H:4) UART
  * SPI
  * Local controller support as a virtual HCI driver

* Raw HCI interface to run Zephyr as a Controller instead of a full Host stack

  * Possible to export HCI over a physical transport
  * ``samples/bluetooth/hci_uart`` sample for HCI over UART
  * ``samples/bluetooth/hci_usb`` sample for HCI over USB

* Verified with multiple popular controllers

* Highly configurable

  * Features, buffer sizes/counts, stack sizes, etc.

Source tree layout
******************

The stack is split up as follows in the source tree:

``subsys/bluetooth/host``
  The host stack. This is where the HCI command & event handling
  as well as connection tracking happens. The implementation of the
  core protocols such as L2CAP, ATT & SMP is also here.

``subsys/bluetooth/controller``
  Bluetooth Controller implementation. Implements the controller-side of
  HCI, the Link Layer as well as access to the radio transceiver.

``include/bluetooth/``
  Public API header files. These are the header files applications need
  to include in order to use Bluetooth functionality.

``drivers/bluetooth/``
  HCI transport drivers. Every HCI transport needs its own driver. E.g.
  the two common types of UART transport protocols (3-Wire & 5-Wire)
  have their own drivers.

``samples/bluetooth/``
  Sample Bluetooth code. This is a good reference to get started with
  Bluetooth application development.

``tests/bluetooth/``
  Test applications. These applications are used to verify the
  functionality of the Bluetooth stack, but are not necessary the best
  source for sample code (see ``samples/bluetooth`` instead).

``doc/subsystems/bluetooth/``
  Extra documentation, such as PICS documents.

Further reading
***************

More information on the stack and its usage can be found
:ref:`here <bluetooth_firmware_arduino_101>` and in the following subsections:

.. toctree::
   :maxdepth: 1

   ../../api/bluetooth.rst
   devel.rst
   qualification.rst
