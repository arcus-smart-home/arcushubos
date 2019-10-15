.. _fifos_v2:

FIFOs
#####

A :dfn:`fifo` is a kernel object that implements a traditional
first in, first out (FIFO) queue, allowing threads and ISRs
to add and remove data items of any size.

.. contents::
    :local:
    :depth: 2

Concepts
********

Any number of fifos can be defined. Each fifo is referenced
by its memory address.

A fifo has the following key properties:

* A **queue** of data items that have been added but not yet removed.
  The queue is implemented as a simple linked list.

A fifo must be initialized before it can be used. This sets its queue to empty.

FIFO data items must be aligned on a 4-byte boundary, as the kernel reserves
the first 32 bits of an item for use as a pointer to the next data item in the
queue. Consequently, a data item that holds N bytes of application data
requires N+4 bytes of memory. There are no alignment or reserved space
requirements for data items if they are added with
:cpp:func:`k_fifo_alloc_put()`, instead additional memory is temporarily
allocated from the calling thread's resource pool.

A data item may be **added** to a fifo by a thread or an ISR.
The item is given directly to a waiting thread, if one exists;
otherwise the item is added to the fifo's queue.
There is no limit to the number of items that may be queued.

A data item may be **removed** from a fifo by a thread. If the fifo's queue
is empty a thread may choose to wait for a data item to be given.
Any number of threads may wait on an empty fifo simultaneously.
When a data item is added, it is given to the highest priority thread
that has waited longest.

.. note::
    The kernel does allow an ISR to remove an item from a fifo, however
    the ISR must not attempt to wait if the fifo is empty.

If desired, **multiple data items** can be added to a fifo in a single operation
if they are chained together into a singly-linked list. This capability can be
useful if multiple writers are adding sets of related data items to the fifo,
as it ensures the data items in each set are not interleaved with other data
items. Adding multiple data items to a fifo is also more efficient than adding
them one at a time, and can be used to guarantee that anyone who removes
the first data item in a set will be able to remove the remaining data items
without waiting.

Implementation
**************

Defining a FIFO
===============

A fifo is defined using a variable of type :c:type:`struct k_fifo`.
It must then be initialized by calling :cpp:func:`k_fifo_init()`.

The following code defines and initializes an empty fifo.

.. code-block:: c

    struct k_fifo my_fifo;

    k_fifo_init(&my_fifo);

Alternatively, an empty fifo can be defined and initialized at compile time
by calling :c:macro:`K_FIFO_DEFINE`.

The following code has the same effect as the code segment above.

.. code-block:: c

    K_FIFO_DEFINE(my_fifo);

Writing to a FIFO
=================

A data item is added to a fifo by calling :cpp:func:`k_fifo_put()`.

The following code builds on the example above, and uses the fifo
to send data to one or more consumer threads.

.. code-block:: c

    struct data_item_t {
        void *fifo_reserved;   /* 1st word reserved for use by fifo */
        ...
    };

    struct data_item_t tx_data;

    void producer_thread(int unused1, int unused2, int unused3)
    {
        while (1) {
            /* create data item to send */
            tx_data = ...

            /* send data to consumers */
            k_fifo_put(&my_fifo, &tx_data);

            ...
        }
    }

Additionally, a singly-linked list of data items can be added to a fifo
by calling :cpp:func:`k_fifo_put_list()` or :cpp:func:`k_fifo_put_slist()`.

Finally, a data item can be added to a fifo with :cpp:func:`k_fifo_alloc_put()`.
With this API, there is no need to reserve space for the kernel's use in
the data item, instead additional memory will be allocated from the calling
thread's resource pool until the item is read.

Reading from a FIFO
===================

A data item is removed from a fifo by calling :cpp:func:`k_fifo_get()`.

The following code builds on the example above, and uses the fifo
to obtain data items from a producer thread,
which are then processed in some manner.

.. code-block:: c

    void consumer_thread(int unused1, int unused2, int unused3)
    {
        struct data_item_t  *rx_data;

        while (1) {
            rx_data = k_fifo_get(&my_fifo, K_FOREVER);

            /* process fifo data item */
            ...
        }
    }

Suggested Uses
**************

Use a fifo to asynchronously transfer data items of arbitrary size
in a "first in, first out" manner.

Configuration Options
*********************

Related configuration options:

* None.

APIs
****

The following fifo APIs are provided by :file:`kernel.h`:

* :c:macro:`K_FIFO_DEFINE`
* :cpp:func:`k_fifo_init()`
* :cpp:func:`k_fifo_alloc_put()`
* :cpp:func:`k_fifo_put()`
* :cpp:func:`k_fifo_put_list()`
* :cpp:func:`k_fifo_put_slist()`
* :cpp:func:`k_fifo_get()`
