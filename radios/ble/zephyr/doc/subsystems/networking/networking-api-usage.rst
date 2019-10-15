.. _networking_api_usage:

Network Connectivity API
########################

Applications can use the connectivity API defined in :file:`net_context.h`
to create a connection, send or receive data, and close a connection.
The same API can be used when working with UDP or TCP data.

The net_context API is similar to the BSD socket API and mapping between these
two is possible. The main difference between net_context API and BSD socket
API is that the net_context API uses the fragmented network buffers (net_buf)
defined in :file:`include/net/buf.h` and BSD socket API uses linear memory buffers.

This example creates a simple server that listens to incoming UDP connections
and sends the received data back. You can download the example application
source file here `connectivity-example-app.c <https://raw.githubusercontent.com/zephyrproject-rtos/zephyr/master/doc/subsystems/networking/connectivity-example-app.c>`_

This example application begins with some initialization. (Use this as an
example; you may need to do things differently in your own application.)

.. literalinclude:: connectivity-example-app.c
    :linenos:
    :language: c
    :lines: 2-54
    :lineno-start: 2

After initialization, first thing application needs to create a context.
Context is similar to a socket.

.. literalinclude:: connectivity-example-app.c
    :linenos:
    :language: c
    :lines: 57-66
    :lineno-start: 57

Then you need to define the local end point for a connection.

.. literalinclude:: connectivity-example-app.c
    :linenos:
    :language: c
    :lines: 69-83
    :lineno-start: 69

Wait until the connection data is received.

.. literalinclude:: connectivity-example-app.c
    :linenos:
    :language: c
    :lines: 86-202
    :lineno-start: 86

Close the context when finished.

.. literalinclude:: connectivity-example-app.c
    :linenos:
    :language: c
    :lines: 204-214
    :lineno-start: 204

.. toctree::
   :maxdepth: 1
