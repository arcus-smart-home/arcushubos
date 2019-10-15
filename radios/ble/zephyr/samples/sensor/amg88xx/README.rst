.. _amg88xx:

AMG88XX Infrared Array Sensor
#############################

Overview
********

This sample application periodically reads the 8x8 temperature array from
the AMG88XX sensor and displays its values to the serial console.
The sample can also be configured to be triggered when the upper threshold
of 27 |deg| Celsius has been reached.

Requirements
************

The Panasonic AMG88XX is an 8x8 (64) pixel infrared array sensor available
only in a SMD design. Here are two sources for easily working with this sensor:

- `AMG88xx Evaluation Kit`_
- `Adafruit AMG8833 8x8 Thermal Camera Sensor`_

We used the Evaluation Kit mounted on FRDM-K64F board.
On the Evaluation Kit, all jumpers except the J11 must be removed.

This sample uses the sensor APIs and the provided driver for the AMG88xx sensor.

Building and Running
********************

This sample reads from the sensor and outputs sensor data to the console every
second. If you want to test the sensor's trigger mode, specify the trigger
configuration in the prj.conf file and connect the interrupt output from the
sensor to your board. For the FRDM-K64F we tested, pin PTC2 must be muxed as
GPIO. You can change the upper threshold values by editing the sample.
The default upper threshold value is 27 |deg| Celsius (80.6 |deg| Fahrenheit).

.. zephyr-app-commands::
   :zephyr-app: samples/sensors/amg88xx
   :board: frdm_k64f
   :goals: build
   :compact:

Sample Output
=============

Sample shows values in |deg| C  multiplied by 100.

.. code-block:: console

   new sample:
   ---|  00    01    02    03    04    05    06    07
   000|02075 01900 02000 01925 02050 01975 01950 01900
   001|02000 02000 02000 01900 02125 02000 02050 02050
   002|02075 01950 01950 01925 02050 01975 02075 02000
   003|02175 02000 01975 01800 02000 02125 01925 02050
   004|01975 02050 02050 02000 02000 01975 01925 02000
   005|02100 02100 02075 02000 02025 02050 02100 02100
   006|01975 02150 02075 02025 02025 02225 02025 02375
   007|02025 02075 02150 02075 02025 02050 02125 02400

The sensor array data is output every second.

References
***********

- https://industrial.panasonic.com/cdbs/www-data/pdf/ADI8000/ADI8000C53.pdf
- https://na.industrial.panasonic.com/sites/default/pidsa/files/downloads/files/grid-eye-high-performance-specifications.pdf

.. _`AMG88xx Evaluation Kit`: https://eu.industrial.panasonic.com/grideye-evalkit
.. _`Adafruit AMG8833 8x8 Thermal Camera Sensor`: https://learn.adafruit.com/adafruit-amg8833-8x8-thermal-camera-sensor/overview
