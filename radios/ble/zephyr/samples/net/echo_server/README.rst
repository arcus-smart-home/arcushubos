.. _echo-server-sample:

Echo Server
###########

Overview
********

The echo-server sample application for Zephyr implements a UDP/TCP server
that complements the echo-client sample application: the echo-server listens
for incoming IPv4 or IPv6 packets (sent by the echo client) and simply sends
them back.

The source code for this sample application can be found at:
:file:`samples/net/echo_server`.

Requirements
************

- :ref:`networking_with_qemu`

Building and Running
********************

There are multiple ways to use this application. One of the most common
usage scenario is to run echo-server application inside QEMU. This is
described in :ref:`networking_with_qemu`.

There are configuration files for different boards and setups in the
echo-server directory:

- :file:`prj.conf`
  Generic config file, normally you should use this.

- :file:`prj_arduino_101.conf`
  Use this for Arduino 101 with external enc28j60 ethernet board.

- :file:`prj_bt.conf`
  Use this for Bluetooth IPSP connectivity.

- :file:`prj_cc2520.conf`
  Use this for devices that have support for IEEE 802.15.4 cc2520 chip.

- :file:`prj_frdm_k64f_cc2520.conf`
  Use this for FRDM-K64F board with external IEEE 802.15.4 cc2520 board.

- :file:`prj_frdm_k64f_mcr20a.conf`
  Use this for FRDM-K64F board with IEEE 802.15.4 mcr20a board.

- :file:`prj_qemu_802154.conf`
  Use this when simulating IEEE 802.15.4 network using two QEMU's that
  are connected together.

- :file:`prj_netusb.conf`
  Use this for Ethernet over USB setup with supported boards. The setup is
  described in :ref:`usb_device_networking_setup`.

Build echo-server sample application like this:

.. zephyr-app-commands::
   :zephyr-app: samples/net/echo_server
   :board: <board to use>
   :conf: <config file to use>
   :goals: build
   :compact:

Make can select the default configuration file based on the BOARD you've
specified automatically so you might not always need to mention it.

Running echo-client in Linux Host
=================================

There is one useful testing scenario that can be used with Linux host.
Here echo-server is run in QEMU and echo-client is run in Linux host.

To use QEMU for testing, follow the :ref:`networking_with_qemu` guide.

Run echo-server application in QEMU:

.. zephyr-app-commands::
   :zephyr-app: samples/net/echo_server
   :host-os: unix
   :board: qemu_x86
   :goals: run
   :compact:

In a terminal window:

.. code-block:: console

    $ sudo ./echo-client -i tap0 2001:db8::1

Note that echo-server must be running in QEMU before you start the
echo-client application in host terminal window.
