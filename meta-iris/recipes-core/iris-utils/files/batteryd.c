/*
 * Copyright 2019 Arcus Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/usbdevice_fs.h>
#include <linux/usb/ch9.h>
#include <irislib.h>


#define INITIAL_DELAY      5
#define PERIODIC_DELAY     2
#define AC_RESTORE_DELAY   1
#define SYSLOG_IDENT       "batteryd"

#define I2CBUS     0
#define CHIP_ADDR  0x24
#define DATA_ADDR  0xA

#define BATTERY_HOLD_ON   0
#define BATTERY_HOLD_OFF  1

#define POWER_STATE_BATTERY  0x00
#define POWER_STATE_AC       0x08
#define POWER_STATE_MASK     0x08

/* USB related defines */
#define USB0_DEV             "/dev/bus/usb/001/001"
#define USB1_DEV             "/dev/bus/usb/002/001"
#define NUM_USB_PORTS        2

struct port_status {
    unsigned short status;
    unsigned short change;
} __attribute__ ((packed));

#define USB_PORT_CONNECTED   0x0001
#define USB_CMD_TIMEOUT      5000    /* milliseconds */
#define USB_PORT_POWER       8

/* Voltage divider details for battery monitoring */
#define MAX_RAW_VOLTAGE      0x0FFF
#define VOLTAGE_DIVIDER_RES1 36500
#define VOLTAGE_DIVIDER_RES2 100000

/* We need a minimum voltage for operation - use that in level calculation */
#define VOLTAGE_IN_MIN       4.2

/* We show a voltage even without any batteries due to the nature of
   the hardware voltage circuitry */
#define VOLTAGE_NO_BATTERIES 1.25

/* When we go below the minimum, reset max so we catch new battery level */
#define BATTERY_RESET_V      5.0

/* Have to take the diode voltage drop into account - this value was
 * empirically derived as we draw too small of a current to be
 * able to use the tables they provide with the part...
 * But, if we are on battery, we will be pulling about .3 amps in
 * normal case (ethernet, but no USB devices) so adjust drop accordingly
 */
#define DIODE_V_DROP_OFF     0.03
#define DIODE_V_DROP_ON      0.25

/* No need to check the voltage every time through the loop!
   Do it once a minute ...
*/
#define VOLTAGE_CHECK_INTERVAL  (60 / PERIODIC_DELAY)

/* Based on CentraLite code that does the following:

LASTSTATE="0x08"
while [ 1 ]; do
  STATUS=$(i2cget -y -f 0 0x24 0xA)
  if [ $STATUS = "0x08" ]; then
    echo "On AC"
    if [ $LASTSTATE = "0x00" ]; then
      echo "Changed from battery to AC"
      sleep 1
    fi
      echo 1 > /sys/class/gpio/gpio117/value
  else
    # turn gpio ON to hold battery power
    echo "On Battery"
    echo 0 > /sys/class/gpio/gpio117/value
  fi
  LASTSTATE=$STATUS
  sleep 1
done

*/


static unsigned char lastState = POWER_STATE_AC;

unsigned char getBatteryState(void)
{
    char   cmd[128];
    FILE   *f;
    int    state;

    snprintf(cmd, sizeof(cmd), "i2cget -y -f %d 0x%X 0x%X",
             I2CBUS, CHIP_ADDR, DATA_ADDR);
    f = popen(cmd, "r");
    if (f != NULL) {
        if (fscanf(f, "%X", &state) != 1) {
            syslog(LOG_ERR, "Error parsing battery state!");
            state = 0xFF;
        }
    } else {
        syslog(LOG_ERR, "Error reading battery state!");
        state = 0xFF;
    }
    pclose(f);
    // Mask out bits we don't care about
    return (unsigned char)(state & POWER_STATE_MASK);
}

void holdBatteryPower(int mode)
{
    char cmd[128];

    snprintf(cmd, sizeof(cmd), "echo %d > %s", mode, BTRY_HOLD_GPIO_VALUE_FILE);
    system(cmd);
}


/*
 * USB disable/enable support
*/


/* Check if a USB port is in the connected state */
static int usb_connected(int port)
{
    int fd;
    char *usb_dev;
    struct usbdevfs_ctrltransfer ct;
    struct port_status ps;

    /* Which port?  input is 0-based */
    if (port == 0) {
        usb_dev = USB0_DEV;
    } else {
        usb_dev = USB1_DEV;
    }
    fd = open(usb_dev, O_RDWR);
    if (fd < 0) {
        syslog(LOG_ERR, "Unable to open USB port %d to check status!", port+1);
        return 0;
    }

    /* Get USB port status */
    ct.bRequestType = USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_OTHER;
    ct.bRequest = USB_REQ_GET_STATUS;
    ct.wValue = 0;
    ct.wIndex = 1;  /* The device is always on port 1 on each USB bus */
    ct.wLength = sizeof(ps);
    ct.timeout = USB_CMD_TIMEOUT;
    ct.data = &ps;
    if (ioctl(fd, USBDEVFS_CONTROL, &ct) == -1) {
        syslog(LOG_ERR, "Error in USB status ioctl (port %d)", port+1);
        close(fd);
        return 0;
    }
    close(fd);

    /* Connected? */
    if (ps.status & USB_PORT_CONNECTED) {
        return 1;
    }
    return 0;
}

/* Disable and turn off power to an USB port */
static int usb_disable(int port)
{
    int fd;
    char *usb_dev;
    struct usbdevfs_ctrltransfer ct;
    struct usbdevfs_ioctl ictl;

    /* Which port?  input is 0-based */
    if (port == 0) {
        usb_dev = USB0_DEV;
    } else {
        usb_dev = USB1_DEV;
    }
    fd = open(usb_dev, O_RDWR);
    if (fd < 0) {
        syslog(LOG_ERR, "Unable to open USB port %d to disable!", port+1);
        return 0;
    }

    /* Disconnect port */
    ictl.ifno = 0;
    ictl.ioctl_code = USBDEVFS_DISCONNECT;
    ictl.data = NULL;
    if ((ioctl(fd, USBDEVFS_IOCTL, &ictl) == -1) && (errno != ENODATA)) {
        syslog(LOG_ERR, "Unable to disconnect USB port %d!", port+1);
        close(fd);
        return 1;
    }

    /* Turn off power */
    ct.bRequest = USB_REQ_CLEAR_FEATURE;
    ct.bRequestType = USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_OTHER;
    ct.wValue = USB_PORT_POWER;
    ct.wIndex = 1;  /* The device is always on port 1 on each USB bus */
    ct.wLength = 0;
    ct.timeout = USB_CMD_TIMEOUT;
    ct.data = NULL;
    if (ioctl(fd, USBDEVFS_CONTROL, &ct) == -1) {
        syslog(LOG_ERR, "Unable to turn off power on USB port %d!", port+1);
        close(fd);
        return 1;
    }
    syslog(LOG_INFO, "USB port %d has been disabled.", port+1);
    close(fd);
    return 0;
}

/* Enable an USB port */
static int usb_enable(int port)
{
    int fd;
    char *usb_dev;
    struct usbdevfs_ioctl ictl;

    /* Which port?  input is 0-based */
    if (port == 0) {
        usb_dev = USB0_DEV;
    } else {
        usb_dev = USB1_DEV;
    }
    fd = open(usb_dev, O_RDWR);
    if (fd < 0) {
        syslog(LOG_ERR, "Unable to open USB port %d to enable!", port+1);
        return 0;
    }

    /* Re-bind port - this has the side-effect of turning the power back on */
    ictl.ifno = 0;
    ictl.ioctl_code = USBDEVFS_CONNECT;
    ictl.data = NULL;
    if (ioctl(fd, USBDEVFS_IOCTL, &ictl) == -1) {
        syslog(LOG_ERR, "Unable to enable USB port %d!", port+1);
        close(fd);
        return 1;
    }
    syslog(LOG_INFO, "USB port %d has been enabled.", port+1);
    return 0;
}

static int usb_port_in_use[NUM_USB_PORTS] = { 0 };

/* Handle USB ports when power goes to battery mode */
static void usb_power_off(void)
{
    int i;

    /* For now, if a USB port is connected, do not disable it -- we will
       fine tune this later once we have 3G support, etc. sorted out */
    for (i = 0; i < NUM_USB_PORTS; i++) {
        if ((usb_port_in_use[i] = usb_connected(i)) == 0) {
            usb_disable(i);
        }
    }
}

/* Handle USB ports when power goes back to mains */
static void usb_power_on(void)
{
    int i;

    /* For now, if a USB port was connected it doesn't need to be
       re-enabled -- we will fine tune this later once we have 3G
       support, etc. sorted out */
    for (i = 0; i < NUM_USB_PORTS; i++) {
        if (usb_port_in_use[i] == 0) {
            usb_enable(i);
        }

        /* Reset state */
        usb_port_in_use[i] = 0;
    }
}


/* Battery daemon - periodically check if we are in battery mode and
   perform necessary actions */
int main(int argc, char** argv)
{
    FILE *f;
    static float max_voltage = 0.0;
    static ulong loop_count = 0;

    /* Turn ourselves into a daemon */
    daemon(1, 1);

    /* Setup syslog */
    openlog(SYSLOG_IDENT, (LOG_CONS | LOG_PID | LOG_PERROR), LOG_DAEMON);

    /* Sleep for a bit to let system complete startup */
    sleep(INITIAL_DELAY);

    /* Get saved maximum voltage, if supported */
    if (IRIS_isBatteryVoltageDetectionSupported()) {
        f = fopen(MAX_BATTERY_VOLTAGE, "r");
        if (f != NULL) {
            fscanf(f, "%f", &max_voltage);
	    fclose(f);
        }
    }

    /* Battery check loop */
    while (1) {
        unsigned char batteryState;

        /* Get battery state */
        batteryState = getBatteryState();

        /* Are we on battery? */
        if (batteryState == POWER_STATE_BATTERY) {
            /* State changed? */
            if (lastState != POWER_STATE_BATTERY) {
                char cmd[128];

                syslog(LOG_ERR, "Running on battery power...");

                /* Need to set GPIO to hold battery power */
                holdBatteryPower(BATTERY_HOLD_ON);

                /* Record time we entered battery mode */
                snprintf(cmd, sizeof(cmd), "date > %s", BTRY_ON_FILE);
                system(cmd);

                /* Power off USB devices, if possible */
                usb_power_off();
            }
        } else {
            /* Were we on battery before? */
            if (lastState == POWER_STATE_BATTERY) {
                syslog(LOG_ERR, "AC power has been restored...");

                /* Remove battery on file */
                remove(BTRY_ON_FILE);

                /* Wait a moment before turning battery hold off */
                sleep(AC_RESTORE_DELAY);

                /* Power on USB devices, if possible */
                usb_power_on();
            }

            /* No longer need to hold battery power */
            holdBatteryPower(BATTERY_HOLD_OFF);
        }

        /* Does the hardware support reading the actual battery voltage?
           Don't do this every time through the loop - since the battery
           voltage may fluctuate, we don't want to be reporting changes
           to the agent every few second!
           But, do make sure we update when the state changes!
        */
        if (IRIS_isBatteryVoltageDetectionSupported() &&
            ((lastState != batteryState) ||
             ((loop_count++ % VOLTAGE_CHECK_INTERVAL) == 0))) {
            int   raw_value = 0;

            /* Read ADC value of battery voltage */
            f = fopen(BTRY_VOLTAGE_VALUE_FILE, "r");
            if (f != NULL) {
                fscanf(f, "%d", &raw_value);
                fclose(f);
            }

            /* Translate to actual voltage */
            if (raw_value > 0) {
                float adc_value = 0.0;
                float current = 0.0;
                float total_voltage = 0.0;
                float level = 0.0;

                /* Value is a 12-bit number - convert to float */
                adc_value = (raw_value * 1.80) / MAX_RAW_VOLTAGE;

                /* We can only read a max of 1.8v on Sitara ADC port,
                   so voltage has been scaled via a voltage divider. */
                current = adc_value / VOLTAGE_DIVIDER_RES1;

                /* May need to make diode value based on current? */
                total_voltage = adc_value + (current * VOLTAGE_DIVIDER_RES2);
                if (batteryState == POWER_STATE_BATTERY) {
                    total_voltage += DIODE_V_DROP_ON;
                } else {
                    total_voltage += DIODE_V_DROP_OFF;
                }

                /* Compare to max - if larger, save new max */
                if (total_voltage > max_voltage) {
                    f = fopen(MAX_BATTERY_VOLTAGE, "w");
                    if (f != NULL) {
                      fprintf(f, "%.2f\n", total_voltage);
                      fclose(f);
                    }
                    max_voltage = total_voltage;
                } else if (total_voltage <= VOLTAGE_IN_MIN) {
                    // Set max to a lower value so we catch battery change
                    max_voltage = BATTERY_RESET_V;
                }

                /* Translate into battery level and update file
                   Return level as "0" if we are below the minimum for
                   operation, or -1 if there appear to be no batteries */
                if (total_voltage <= VOLTAGE_NO_BATTERIES) {
                    level = -1;
                } else if (total_voltage <= VOLTAGE_IN_MIN) {
                    level = 0;
                } else {
                    level = ((total_voltage - VOLTAGE_IN_MIN) /
                             (max_voltage - VOLTAGE_IN_MIN)) * 100;
                }
                f = fopen(BTRY_LEVEL_FILE, "w");
                if (f != NULL) {
                    fprintf(f, "%.2f\n", level);
                    fclose(f);
                }

                /* Save voltage to file */
                f = fopen(BTRY_VOLTAGE_FILE, "w");
                if (f != NULL) {
                    fprintf(f, "%.2f\n", total_voltage);
                    fclose(f);
                }
            }
        }

        /* Record last state */
        lastState = batteryState;

        /* Try again in a while ... */
        sleep(PERIODIC_DELAY);
    }

    /* Should never get here ... */
    exit(0);
}
