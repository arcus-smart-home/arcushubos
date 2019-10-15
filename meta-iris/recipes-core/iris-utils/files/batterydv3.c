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
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <irislib.h>


#define INITIAL_DELAY      5
#define PERIODIC_DELAY     2
#define SYSLOG_IDENT       "batteryd"

#define POWER_STATE_BATTERY  0x00
#define POWER_STATE_AC       0x01
#define MAX_RAW_VOLTAGE      0x0FFF

/* USB related defines */
#define USB0_DEV             "/dev/bus/usb/001/001"

struct port_status {
    unsigned short status;
    unsigned short change;
} __attribute__ ((packed));

#define USB_PORT_CONNECTED   0x0001
#define USB_CMD_TIMEOUT      5000    /* milliseconds */
#define USB_PORT_POWER       8

/* We need a minimum voltage for operation - use that in level calculation */
#define VOLTAGE_IN_MIN       2.68

/* We need a maximum charging voltage for level calculation as well */
#define VOLTAGE_IN_MAX       4.15

/* Charging voltage for battery circuit */
#define CRG_VOLTAGE_BASE     2.50
#define CRG_VOLTAGE_MULT     2.50

/* No need to check the voltage every time through the loop!
   Do it once a minute ...
*/
#define VOLTAGE_CHECK_INTERVAL  (60 / PERIODIC_DELAY)

static unsigned char lastState = POWER_STATE_AC;
static int usb_port_in_use = 0;

typedef enum LteRestartStates {
    IDLE = 0,
    START,
    START_WAIT,
    DISCONNECT,
    DISCONNECT_WAIT,
    RECONNECT
} LteRestartStates_t;
static LteRestartStates_t lte_restart_state = IDLE;

#define I2C1_DEV            "/dev/i2c-1"
#define SY6991_SLAVE_ADDR   0x6A
#define SY6410_SLAVE_ADDR   0x30

#define VIN_PRESENT         0x01
#define BATTERY_MISSING     0x04
#define BOOST               0x40
#define STATUS_FIELD        0x30
#define STATUS_READY        0x00
#define STATUS_CHARGING     0x10
#define STATUS_DONE         0x20
#define STATUS_FAULT        0x30

unsigned char rdaddr0[1] =  {0x00};
unsigned char rdaddr2[1] =  {0x02};
unsigned char rdaddr3[1] =  {0x03};
unsigned char rdaddr4[1] =  {0x04};
unsigned char rdaddr5[1] =  {0x05};

unsigned char getBatteryState(void)
{
    int    fd;
    unsigned char data = 0;

    fd = open(I2C1_DEV, O_RDWR);
    if (fd < 0) {
        syslog(LOG_ERR, "Error opening %s!", I2C1_DEV);
        return VIN_PRESENT;
    }

    if (ioctl(fd, I2C_SLAVE_FORCE, SY6991_SLAVE_ADDR) < 0) {
        syslog(LOG_ERR, "Error setting SY6991 slave address!");
        close(fd);
        return VIN_PRESENT;
    }

    // Get status info
    write(fd, rdaddr0, 1);
    read(fd, &data, 1);
    close(fd);
    return data;
}

/*
 * USB disable/enable support
*/


/* Check if the external USB port is in the connected state */
static int usb_connected()
{
    int fd;
    struct usbdevfs_ctrltransfer ct;
    struct port_status ps;

    fd = open(USB0_DEV, O_RDWR);
    if (fd < 0) {
        syslog(LOG_ERR, "Unable to open USB port to check status!");
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
        syslog(LOG_ERR, "Error in USB status ioctl");
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

/* Disable / turn off power to external USB port */
static int usb_disable()
{
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "echo 0 > %s", PWR_EN_USB_VALUE_FILE);
    system(cmd);
    syslog(LOG_INFO, "USB port has been disabled.");
    return 0;
}

/* Enable the external USB port power */
static int usb_enable()
{
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "echo 1 > %s", PWR_EN_USB_VALUE_FILE);
    system(cmd);
    syslog(LOG_INFO, "USB port has been enabled.");
    return 0;
}

/* Handle USB ports when power goes to battery mode */
static void usb_power_off(void)
{
    /* For now, if a USB port is connected, do not disable it -- we will
       fine tune this later once we have 3G support, etc. sorted out */
    if ((usb_port_in_use = usb_connected()) == 0) {
        usb_disable();
    }
}

/* Handle USB ports when power goes back to mains */
static void usb_power_on(void)
{
    /* For now, if a USB port was connected it doesn't need to be
       re-enabled -- we will fine tune this later once we have 3G
       support, etc. sorted out */
    if (usb_port_in_use == 0) {
        usb_enable();
    }

    /* Reset state */
    usb_port_in_use = 0;
}

/* Battery daemon - periodically check if we are in battery mode and
   perform necessary actions */
int main(int argc, char** argv)
{
    FILE *f;
    int fd;
    static ulong loop_count = 0;

    /* Turn ourselves into a daemon */
    daemon(1, 1);

    /* Setup syslog */
    openlog(SYSLOG_IDENT, (LOG_CONS | LOG_PID | LOG_PERROR), LOG_DAEMON);

    /* Set initial values in case battery is missing */
    f = fopen(BTRY_LEVEL_FILE, "w");
    if (f != NULL) {
        fprintf(f, "0.0\n");
        fclose(f);
    }
    f = fopen(BTRY_VOLTAGE_FILE, "w");
    if (f != NULL) {
        fprintf(f, "0.0\n");
        fclose(f);
    }

    /* Sleep for a bit to let system complete startup */
    sleep(INITIAL_DELAY);

    /* Battery check loop */
    while (1) {
        unsigned char batteryState;
        char          cmd[128];

        /* Do we need to restart the LTE connection due to a power bounce? */
        switch (lte_restart_state) {
        case IDLE:
        default:
            break;
        case START:
            /* Reset port as power loss sometimes leaves modem in limbo */
            usb_disable();
            usleep(10000);
            usb_enable();
            lte_restart_state = START_WAIT;
            break;
        case START_WAIT:
            /* Wait for dongle to come back */
            if (access(LTE_DONGLE_FILE, F_OK) == 0) {
                lte_restart_state = DISCONNECT;
            }
            break;
        case DISCONNECT:
            syslog(LOG_INFO, "Disconnecting LTE due to power bounce...");
            snprintf(cmd, sizeof(cmd), "echo disconnect > %s",
                     LTE_CONTROL_FILE);
            system(cmd);
            lte_restart_state = DISCONNECT_WAIT;
            break;
        case DISCONNECT_WAIT:
            /* Wait for dongle to come back */
            if (access(LTE_DONGLE_FILE, F_OK) == 0) {
                lte_restart_state = RECONNECT;
            }
            break;
        case RECONNECT:
            syslog(LOG_INFO, "Reconnecting LTE due to power bounce...");
            snprintf(cmd, sizeof(cmd), "echo connect > %s",
                     LTE_CONTROL_FILE);
            system(cmd);
            lte_restart_state = IDLE;
            break;
        }

        /* Get battery state */
        batteryState = getBatteryState();

        /* Are we on battery? */
        if ((batteryState & VIN_PRESENT) == 0) {
            /* State changed? */
            if (lastState & VIN_PRESENT) {

                syslog(LOG_ERR, "Running on battery power...");

                /* Record time we entered battery mode */
                snprintf(cmd, sizeof(cmd), "date > %s", BTRY_ON_FILE);
                system(cmd);

                /* The GS hardware will briefly drop power to the USB
                   ports when switching to battery.  This may cause the
                   LTE modem to reset.  So, we need to re-connect if we
                   were connected (and not bother to drop USB power...)
                */
                if (IRIS_isLTEConnected()) {
                    usb_port_in_use = 1;

                    /* Start reconnect state machine */
                    lte_restart_state = START;
                } else {
                    /* Power off USB devices, if possible */
                    usb_power_off();
                }
            }
        } else {
            /* Were we on battery before? */
            if ((lastState & VIN_PRESENT) == 0) {
                syslog(LOG_ERR, "AC power has been restored...");

                /* Remove battery on file */
                remove(BTRY_ON_FILE);

                /* Power on USB devices, if possible */
                usb_power_on();
            }
        }

        /* Record new charging state */
        if (lastState != batteryState) {
            f = fopen(CHARGE_STATE_FILE, "w");
            if (f != NULL) {
                if (batteryState & BOOST) {
                    fprintf(f, "Boost\n");
                } else if (batteryState & BATTERY_MISSING) {
                    fprintf(f, "Missing\n");
                } else if ((batteryState & STATUS_FIELD) == STATUS_READY) {
                    fprintf(f, "Ready\n");
                } else if ((batteryState & STATUS_FIELD) == STATUS_CHARGING) {
                    fprintf(f, "Charging\n");
                } else if ((batteryState & STATUS_FIELD) == STATUS_DONE) {
                    fprintf(f, "Done\n");
                } else if ((batteryState & STATUS_FIELD) == STATUS_FAULT) {
                    fprintf(f, "Fault\n");
                }
                fclose(f);
            }
        }

        /* Don't do this every time through the loop - since the battery
           voltage may fluctuate, we don't want to be reporting changes
           to the agent every few second!
           But, do make sure we update when the state changes!
        */
        if ((lastState != batteryState) ||
             ((loop_count++ % VOLTAGE_CHECK_INTERVAL) == 0)) {
            int   raw_value = 0;

            fd = open(I2C1_DEV, O_RDWR);
            if (fd >= 0) {
                if (ioctl(fd, I2C_SLAVE_FORCE, SY6410_SLAVE_ADDR) >= 0) {
                    unsigned char data = 0x00;
                    int charge = 0;
                    float charge_level = 0;

                    /* Read ADC value of battery voltage */
                    write(fd, rdaddr2, 1);
                    read(fd, &data, 1);
                    raw_value = data;
                    write(fd, rdaddr3, 1);
                    read(fd, &data, 1);
                    raw_value = ((raw_value & 0xFF) << 8) | data;

                    // Record current charge level
                    write(fd, rdaddr4, 1);
                    read(fd, &data, 1);
                    charge = data;
                    write(fd, rdaddr5, 1);
                    read(fd, &data, 1);
                    charge = (charge << 8) | data;
                    charge_level = ((charge * 100) / 0xFFFF);
                    f = fopen(CHARGE_LEVEL_FILE, "w");
                    if (f != NULL) {
		        fprintf(f, "%.2f\n", charge_level);
                        fclose(f);
                    }
                }
                close(fd);
            }

            /* Translate to actual voltage */
            if (raw_value > 0) {
                float total_voltage = 0.0;
                float level = 0.0;

                /* Value is a 12-bit number - convert to float */
                total_voltage = CRG_VOLTAGE_BASE +
                  ((raw_value * CRG_VOLTAGE_MULT) / MAX_RAW_VOLTAGE);

                /* Translate into battery level and update file
                   Return level as "0" if we are below the minimum for
                   operation */
                if (total_voltage <= VOLTAGE_IN_MIN) {
                    level = 0;
                } else {
                    level = ((total_voltage - VOLTAGE_IN_MIN) /
                             (VOLTAGE_IN_MAX - VOLTAGE_IN_MIN)) * 100;
                }
                /* Trim if above 100 */
                if (level > 100) level = 100.0;
                /* Save level */
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
