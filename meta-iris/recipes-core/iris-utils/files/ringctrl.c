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
#include <string.h>
#include <stdio.h>
#include <irisdefs.h>

/* Possible LED ring modes */
typedef enum {
  RING_OFF = 0,
  RING_BLINK_ONCE_SHORT,
  RING_BLINK_ONCE_LONG,
  RING_BLINK_SLOW,
  RING_BLINK_FAST,
  RING_BLINK_TRIPLE,
  RING_SOLID,
  RING_ROTATE_SLOW,
  RING_ROTATE_FAST,
  RING_PULSE,
  RING_WAVE,
  RING_TICK_DOWN,
  RING_TICK_UP
} led_setting_t;

/* LED modes - from software requirements */
typedef enum {
  BOOT_POST = 0,
  BOOT_LINUX,
  BOOT_AGENT,
  NORMAL_UNASSOC_CONN_PRI,
  NORMAL_UNASSOC_CONN_PRI_BATT,
  NORMAL_UNASSOC_CONN_BACKUP,
  NORMAL_UNASSOC_CONN_BACKUP_BATT,
  NORMAL_UNASSOC_DISCONN,
  NORMAL_UNASSOC_DISCONN_BATT,
  NORMAL_ASSOC_CONN_PRI,
  NORMAL_ASSOC_CONN_PRI_BATT,
  NORMAL_ASSOC_CONN_BACKUP,
  NORMAL_ASSOC_CONN_BACKUP_BATT,
  NORMAL_ASSOC_DISCONN,
  NORMAL_ASSOC_DISCONN_BATT,
  NORMAL_PAIRING_CONN_PRI,
  NORMAL_PAIRING_CONN_PRI_BATT,
  NORMAL_PAIRING_CONN_BACKUP,
  NORMAL_PAIRING_CONN_BACKUP_BATT,
  NORMAL_PAIRING_DISCONN,
  NORMAL_PAIRING_DISCONN_BATT,
  UPGRADE_DECRYPT,
  UPGRADE_DECRYPT_ERR,
  UPGRADE_UNPACK,
  UPGRADE_UNPACK_ERR,
  UPGRADE_BOOTLOADER,
  UPGRADE_BOOTLOADER_ERR,
  UPGRADE_KERNEL,
  UPGRADE_KERNEL_ERR,
  UPGRADE_ROOTFS,
  UPGRADE_ROOTFS_ERR,
  UPGRADE_ZIGBEE,
  UPGRADE_ZIGBEE_ERR,
  UPGRADE_ZWAVE,
  UPGRADE_ZWAVE_ERR,
  UPGRADE_BTE,
  UPGRADE_BTE_ERR,
  SHUTDOWN,
  BUTTON_REBOOT,
  BUTTON_SOFT_RESET,
  BUTTON_FACTORY_DEFAULT,
  ALL_ON,
  ALL_OFF,
  /* These are additions for the v3 hub, may be dups of above? */
  FIRST_BOOTUP,
  ETHERNET_INSERTED,
  WIFI_IN_PROGRESS,
  WIFI_SUCCESS,
  WIFI_FAILURE,
  INTERNET_IN_PROGRESS,
  INTERNET_SUCCESS,
  INTERNET_FAILURE,
  CLOUD_IN_PROGRESS,
  CLOUD_SUCCESS,
  CLOUD_FAILURE,
  REGISTER_IN_PROGRESS,
  REGISTER_SUCCESS,
  REGISTER_FAILURE,
  DEVICE_PAIRED,
  DEVICE_REMOVED,
  BUTTON_PRESS_NORMAL,
  BUTTON_PRESS_STATUS,
  BUTTON_PRESS_BATTERY,
  BUTTON_PRESS_OFFLINE,
  BUTTON_PRESS_BACKUP,
  BUTTON_PRESS_BACKUP_BATT,
  BUTTON_PRESS_OFFLINE_BATT,
  BUTTON_PRESS_BACKUP_OFFLINE,
  BUTTON_PRESS_WIFI_RECONNECT,
  DEREGISTERING,
  FACTORY_RESET_ACK,
  TURNING_OFF,
  ALARM_GRACE_ENTER,
  ALARM_GRACE_EXIT,
  ALARM_OFF,
  ALARM_ON,
  ALARM_PARTIAL,
  ALARM_FAILURE,
  ALARMING_SECURITY,
  ALARMING_SECURITY_BATT,
  ALARMING_LOCAL,
  ALARMING_LOCAL_BATT,
  ALARMING_PANIC,
  ALARMING_PANIC_BATT,
  ALARMING_PANIC_MON,
  ALARMING_PANIC_MON_BATT,
  ALARMING_PANIC_LOCAL,
  ALARMING_PANIC_LOCAL_BATT,
  ALARMING_SMOKE,
  ALARMING_SMOKE_BATT,
  ALARMING_CO,
  ALARMING_CO_BATT,
  ALARMING_LEAK,
  ALARMING_LEAK_BATT,
  ALARMING_CARE,
  ALARMING_CARE_BATT,
  DOOR_CHIME,
  UNKNOWN_LED_MODE
} leds_mode_t;


/* Constants */
#define RING_APP                 "ledRing"
#define MAX_STATE_NAME_LENGTH    24
#define COLOR_OFF                0x000000
#define COLOR_WHITE              0x402010
#define COLOR_RED                0x640000
#define COLOR_GREEN              0x006400
#define COLOR_BLUE               0x000064
#define COLOR_BLUE_TICK          0x000030 // Lower power to avoid bleed
#define COLOR_PURPLE             0x640064
#define COLOR_PINK               0x64000A
#define COLOR_TEAL               0x006432
#define COLOR_YELLOW             0x401800

/* State of all LEDs */
typedef struct led_state {
  int color;
  led_setting_t mode;
  int duration;
} led_state_t;

static leds_mode_t lastMode = BOOT_POST;

/* LED states - from software requirements */
static led_state_t states[] = {
  [BOOT_POST]                       = { COLOR_PURPLE, RING_SOLID, 0 },
  [BOOT_LINUX]                      = { COLOR_PURPLE, RING_SOLID, 0 },
  [BOOT_AGENT]                      = { COLOR_PURPLE, RING_ROTATE_SLOW, 0 },
  [NORMAL_UNASSOC_CONN_PRI]         = { COLOR_OFF, RING_OFF, 0 },
  [NORMAL_UNASSOC_CONN_PRI_BATT]    = { COLOR_OFF, RING_OFF, 0 },
  [NORMAL_UNASSOC_CONN_BACKUP]      = { COLOR_OFF, RING_OFF, 0 },
  [NORMAL_UNASSOC_CONN_BACKUP_BATT] = { COLOR_OFF, RING_OFF, 0 },
  [NORMAL_UNASSOC_DISCONN]          = { COLOR_OFF, RING_OFF, 0 },
  [NORMAL_UNASSOC_DISCONN_BATT]     = { COLOR_OFF, RING_OFF, 0 },
  [NORMAL_ASSOC_CONN_PRI]           = { COLOR_OFF, RING_OFF, 0 },
  [NORMAL_ASSOC_CONN_PRI_BATT]      = { COLOR_OFF, RING_OFF, 0 },
  [NORMAL_ASSOC_CONN_BACKUP]        = { COLOR_OFF, RING_OFF, 0 },
  [NORMAL_ASSOC_CONN_BACKUP_BATT]   = { COLOR_OFF, RING_OFF, 0 },
  [NORMAL_ASSOC_DISCONN]            = { COLOR_OFF, RING_OFF, 0 },
  [NORMAL_ASSOC_DISCONN_BATT]       = { COLOR_OFF, RING_OFF, 0 },
  [NORMAL_PAIRING_CONN_PRI]         = { COLOR_GREEN, RING_ROTATE_SLOW, 0 },
  [NORMAL_PAIRING_CONN_PRI_BATT]    = { COLOR_GREEN, RING_ROTATE_SLOW, 0 },
  [NORMAL_PAIRING_CONN_BACKUP]      = { COLOR_GREEN, RING_ROTATE_SLOW, 0 },
  [NORMAL_PAIRING_CONN_BACKUP_BATT] = { COLOR_GREEN, RING_ROTATE_SLOW, 0 },
  [NORMAL_PAIRING_DISCONN]          = { COLOR_BLUE, RING_PULSE, 0 },
  [NORMAL_PAIRING_DISCONN_BATT]     = { COLOR_BLUE, RING_PULSE, 0 },
  [UPGRADE_DECRYPT]                 = { COLOR_WHITE, RING_ROTATE_SLOW, 10 },
  [UPGRADE_DECRYPT_ERR]             = { COLOR_RED, RING_SOLID, 0 },
  [UPGRADE_UNPACK]                  = { COLOR_WHITE, RING_ROTATE_SLOW, 10 },
  [UPGRADE_UNPACK_ERR]              = { COLOR_RED, RING_SOLID, 0 },
  [UPGRADE_BOOTLOADER]              = { COLOR_WHITE, RING_ROTATE_SLOW, 10 },
  [UPGRADE_BOOTLOADER_ERR]          = { COLOR_RED, RING_SOLID, 0 },
  [UPGRADE_KERNEL]                  = { COLOR_WHITE, RING_ROTATE_SLOW, 10 },
  [UPGRADE_KERNEL_ERR]              = { COLOR_RED, RING_SOLID, 0 },
  [UPGRADE_ROOTFS]                  = { COLOR_WHITE, RING_ROTATE_SLOW, 10 },
  [UPGRADE_ROOTFS_ERR]              = { COLOR_RED, RING_SOLID, 0 },
  [UPGRADE_ZIGBEE]                  = { COLOR_WHITE, RING_ROTATE_SLOW, 10 },
  [UPGRADE_ZIGBEE_ERR]              = { COLOR_RED, RING_SOLID, 0 },
  [UPGRADE_ZWAVE]                   = { COLOR_WHITE, RING_ROTATE_SLOW, 10 },
  [UPGRADE_ZWAVE_ERR]               = { COLOR_RED, RING_SOLID, 0 },
  [UPGRADE_BTE]                     = { COLOR_WHITE, RING_ROTATE_SLOW, 10 },
  [UPGRADE_BTE_ERR]                 = { COLOR_RED, RING_SOLID, 0 },
  [SHUTDOWN]                        = { COLOR_GREEN, RING_SOLID, 5 },
  [BUTTON_REBOOT]                   = { COLOR_GREEN, RING_SOLID, 5 },
  [BUTTON_SOFT_RESET]               = { COLOR_GREEN, RING_SOLID, 5 },
  [BUTTON_FACTORY_DEFAULT]          = { COLOR_PINK, RING_SOLID, 15 },
  [ALL_ON]                          = { COLOR_WHITE, RING_OFF, 0 },
  [ALL_OFF]                         = { COLOR_OFF, RING_SOLID, 0 },
  [FIRST_BOOTUP]                    = { COLOR_BLUE, RING_ROTATE_SLOW, 0 },
  [ETHERNET_INSERTED]               = { COLOR_BLUE, RING_ROTATE_SLOW, 0 },
  [WIFI_IN_PROGRESS]                = { COLOR_BLUE, RING_ROTATE_SLOW, 120 },
  [WIFI_SUCCESS]                    = { COLOR_BLUE, RING_ROTATE_SLOW, 0 },
  [WIFI_FAILURE]                    = { COLOR_PINK, RING_ROTATE_SLOW, 0 },
  [INTERNET_IN_PROGRESS]            = { COLOR_BLUE, RING_ROTATE_SLOW, 60 },
  [INTERNET_SUCCESS]                = { COLOR_BLUE, RING_ROTATE_SLOW, 0 },
  [INTERNET_FAILURE]                = { COLOR_PINK, RING_SOLID, 0 },
  [CLOUD_IN_PROGRESS]               = { COLOR_BLUE, RING_ROTATE_SLOW, 60 },
  [CLOUD_SUCCESS]                   = { COLOR_BLUE, RING_SOLID, 0 },
  [CLOUD_FAILURE]                   = { COLOR_PINK, RING_PULSE, 0 },
  [REGISTER_IN_PROGRESS]            = { COLOR_BLUE, RING_SOLID, 60 },
  [REGISTER_SUCCESS]                = { COLOR_GREEN, RING_BLINK_TRIPLE, 0 },
  [REGISTER_FAILURE]                = { COLOR_PINK, RING_PULSE, 0 },
  [DEVICE_PAIRED]                   = { COLOR_GREEN, RING_BLINK_TRIPLE, 0 },
  [DEVICE_REMOVED]                  = { COLOR_GREEN, RING_BLINK_ONCE_LONG, 0 },
  [BUTTON_PRESS_NORMAL]             = { COLOR_GREEN, RING_SOLID, 5 },
  [BUTTON_PRESS_STATUS]             = { COLOR_GREEN, RING_SOLID, 5 },
  [BUTTON_PRESS_BATTERY]            = { COLOR_YELLOW, RING_SOLID, 5 },
  [BUTTON_PRESS_OFFLINE]            = { COLOR_PINK, RING_SOLID, 5 },
  [BUTTON_PRESS_BACKUP]             = { COLOR_RED, RING_SOLID, 5 },
  [BUTTON_PRESS_BACKUP_BATT]        = { COLOR_YELLOW, RING_SOLID, 5 },
  [BUTTON_PRESS_OFFLINE_BATT]       = { COLOR_PINK, RING_SOLID, 5 },
  [BUTTON_PRESS_BACKUP_OFFLINE]     = { COLOR_PINK, RING_SOLID, 5 },
  [BUTTON_PRESS_WIFI_RECONNECT]     = { COLOR_GREEN, RING_SOLID, 5 },
  [DEREGISTERING]                   = { COLOR_GREEN, RING_SOLID, 10 },
  [FACTORY_RESET_ACK]               = { COLOR_GREEN, RING_SOLID, 15 },
  [TURNING_OFF]                     = { COLOR_GREEN, RING_SOLID, 5 },
  [ALARM_GRACE_ENTER]               = { COLOR_BLUE_TICK, RING_TICK_DOWN, 0 },
  [ALARM_GRACE_EXIT]                = { COLOR_BLUE_TICK, RING_TICK_DOWN, 0 },
  [ALARM_OFF]                       = { COLOR_GREEN, RING_BLINK_ONCE_LONG, 0 },
  [ALARM_ON]                        = { COLOR_BLUE, RING_BLINK_ONCE_LONG, 0 },
  [ALARM_PARTIAL]                   = { COLOR_BLUE, RING_BLINK_ONCE_LONG, 0 },
  [ALARM_FAILURE]                   = { COLOR_PINK, RING_SOLID, 5 },
  [ALARMING_SECURITY]               = { COLOR_BLUE, RING_PULSE, 0 },
  [ALARMING_SECURITY_BATT]          = { COLOR_BLUE, RING_PULSE, 120 },
  [ALARMING_LOCAL]                  = { COLOR_BLUE, RING_PULSE, 0 },
  [ALARMING_LOCAL_BATT]             = { COLOR_BLUE, RING_PULSE, 120 },
  [ALARMING_PANIC]                  = { COLOR_WHITE, RING_PULSE, 0 },
  [ALARMING_PANIC_BATT]             = { COLOR_WHITE, RING_PULSE, 120 },
  [ALARMING_PANIC_MON]              = { COLOR_WHITE, RING_PULSE, 0 },
  [ALARMING_PANIC_MON_BATT]         = { COLOR_WHITE, RING_PULSE, 120 },
  [ALARMING_PANIC_LOCAL]            = { COLOR_WHITE, RING_PULSE, 0 },
  [ALARMING_PANIC_LOCAL_BATT]       = { COLOR_WHITE, RING_PULSE, 120 },
  [ALARMING_SMOKE]                  = { COLOR_RED, RING_PULSE, 0 },
  [ALARMING_SMOKE_BATT]             = { COLOR_RED, RING_PULSE, 120 },
  [ALARMING_CO]                     = { COLOR_RED, RING_PULSE, 0 },
  [ALARMING_CO_BATT]                = { COLOR_RED, RING_PULSE, 120 },
  [ALARMING_LEAK]                   = { COLOR_TEAL, RING_PULSE, 0 },
  [ALARMING_LEAK_BATT]              = { COLOR_TEAL, RING_PULSE, 120 },
  [ALARMING_CARE]                   = { COLOR_PURPLE, RING_WAVE, 0 },
  [ALARMING_CARE_BATT]              = { COLOR_PURPLE, RING_WAVE, 120 },
  [DOOR_CHIME]                      = { COLOR_PURPLE, RING_BLINK_ONCE_LONG, 0 },
  [UNKNOWN_LED_MODE]                = { COLOR_OFF, RING_OFF, 0}
};

/* What to call LED states for mode selection */
static char *state_names[] = {
  [BOOT_POST]                       = "boot-post",
  [BOOT_LINUX]                      = "boot-linux",
  [BOOT_AGENT]                      = "boot-agent",
  [NORMAL_UNASSOC_CONN_PRI]         = "unassoc-conn-pri",
  [NORMAL_UNASSOC_CONN_PRI_BATT]    = "unassoc-conn-pri-batt",
  [NORMAL_UNASSOC_CONN_BACKUP]      = "unassoc-conn-backup",
  [NORMAL_UNASSOC_CONN_BACKUP_BATT] = "unassoc-conn-backup-batt",
  [NORMAL_UNASSOC_DISCONN]          = "unassoc-disconn",
  [NORMAL_UNASSOC_DISCONN_BATT]     = "unassoc-disconn-batt",
  [NORMAL_ASSOC_CONN_PRI]           = "assoc-conn-pri",
  [NORMAL_ASSOC_CONN_PRI_BATT]      = "assoc-conn-pri-batt",
  [NORMAL_ASSOC_CONN_BACKUP]        = "assoc-conn-backup",
  [NORMAL_ASSOC_CONN_BACKUP_BATT]   = "assoc-conn-backup-batt",
  [NORMAL_ASSOC_DISCONN]            = "assoc-disconn",
  [NORMAL_ASSOC_DISCONN_BATT]       = "assoc-disconn-batt",
  [NORMAL_PAIRING_CONN_PRI]         = "pairing-conn-pri",
  [NORMAL_PAIRING_CONN_PRI_BATT]    = "pairing-conn-pri-batt",
  [NORMAL_PAIRING_CONN_BACKUP]      = "pairing-conn-backup",
  [NORMAL_PAIRING_CONN_BACKUP_BATT] = "pairing-conn-backup-batt",
  [NORMAL_PAIRING_DISCONN]          = "pairing-disconn",
  [NORMAL_PAIRING_DISCONN_BATT]     = "pairing-disconn-batt",
  [UPGRADE_DECRYPT]                 = "upgrade-decrypt",
  [UPGRADE_DECRYPT_ERR]             = "upgrade-decrypt-err",
  [UPGRADE_UNPACK]                  = "upgrade-unpack",
  [UPGRADE_UNPACK_ERR]              = "upgrade-unpack-err",
  [UPGRADE_BOOTLOADER]              = "upgrade-bootloader",
  [UPGRADE_BOOTLOADER_ERR]          = "upgrade-bootloader-err",
  [UPGRADE_KERNEL]                  = "upgrade-kernel",
  [UPGRADE_KERNEL_ERR]              = "upgrade-kernel-err",
  [UPGRADE_ROOTFS]                  = "upgrade-rootfs",
  [UPGRADE_ROOTFS_ERR]              = "upgrade-rootfs-err",
  [UPGRADE_ZIGBEE]                  = "upgrade-zigbee",
  [UPGRADE_ZIGBEE_ERR]              = "upgrade-zigbee-err",
  [UPGRADE_ZWAVE]                   = "upgrade-zwave",
  [UPGRADE_ZWAVE_ERR]               = "upgrade-zwave-err",
  [UPGRADE_BTE]                     = "upgrade-bte",
  [UPGRADE_BTE_ERR]                 = "upgrade-bte-err",
  [SHUTDOWN]                        = "shutdown",
  [BUTTON_REBOOT]                   = "button-reboot",
  [BUTTON_SOFT_RESET]               = "button-soft-reset",
  [BUTTON_FACTORY_DEFAULT]          = "button-factory-default",
  [ALL_ON]                          = "all-on",
  [ALL_OFF]                         = "all-off",
  [FIRST_BOOTUP]                    = "first-bootup",
  [ETHERNET_INSERTED]               = "ethernet-inserted",
  [WIFI_IN_PROGRESS]                = "wifi-in-progress",
  [WIFI_SUCCESS]                    = "wifi-success",
  [WIFI_FAILURE]                    = "wifi-failure",
  [INTERNET_IN_PROGRESS]            = "internet-in-progress",
  [INTERNET_SUCCESS]                = "internet-success",
  [INTERNET_FAILURE]                = "internet-failure",
  [CLOUD_IN_PROGRESS]               = "cloud-in-progress",
  [CLOUD_SUCCESS]                   = "cloud-success",
  [CLOUD_FAILURE]                   = "cloud-failure",
  [REGISTER_IN_PROGRESS]            = "register-in-progress",
  [REGISTER_SUCCESS]                = "register-success",
  [REGISTER_FAILURE]                = "register-failure",
  [DEVICE_PAIRED]                   = "device-paired",
  [DEVICE_REMOVED]                  = "device-removed",
  [BUTTON_PRESS_NORMAL]             = "button-press-normal",
  [BUTTON_PRESS_STATUS]             = "button-press-status",
  [BUTTON_PRESS_BATTERY]            = "button-press-battery",
  [BUTTON_PRESS_OFFLINE]            = "button-press-offline",
  [BUTTON_PRESS_BACKUP]             = "button-press-backup",
  [BUTTON_PRESS_BACKUP_BATT]        = "button-press-backup-batt",
  [BUTTON_PRESS_OFFLINE_BATT]       = "button-press-offline-batt",
  [BUTTON_PRESS_BACKUP_OFFLINE]     = "button-press-backup-offline",
  [BUTTON_PRESS_WIFI_RECONNECT]     = "button-press-wifi-reconnect",
  [DEREGISTERING]                   = "deregistering",
  [FACTORY_RESET_ACK]               = "factory-reset-ack",
  [TURNING_OFF]                     = "turning-off",
  [ALARM_GRACE_ENTER]               = "alarm-grace-enter",
  [ALARM_GRACE_EXIT]                = "alarm-grace-exit",
  [ALARM_OFF]                       = "alarm-off",
  [ALARM_ON]                        = "alarm-on",
  [ALARM_PARTIAL]                   = "alarm-partial",
  [ALARM_FAILURE]                   = "alarm-failure",
  [ALARMING_SECURITY]               = "alarming-security",
  [ALARMING_SECURITY_BATT]          = "alarming-security-batt",
  [ALARMING_LOCAL]                  = "alarming-local",
  [ALARMING_LOCAL_BATT]             = "alarming-local-batt",
  [ALARMING_PANIC]                  = "alarming-panic",
  [ALARMING_PANIC_BATT]             = "alarming-panic-batt",
  [ALARMING_PANIC_MON]              = "alarming-panic-mon",
  [ALARMING_PANIC_MON_BATT]         = "alarming-panic-mon-batt",
  [ALARMING_PANIC_LOCAL]            = "alarming-panic-local",
  [ALARMING_PANIC_LOCAL_BATT]       = "alarming-panic-local-batt",
  [ALARMING_SMOKE]                  = "alarming-smoke",
  [ALARMING_SMOKE_BATT]             = "alarming-smoke-batt",
  [ALARMING_CO]                     = "alarming-co",
  [ALARMING_CO_BATT]                = "alarming-co-batt",
  [ALARMING_LEAK]                   = "alarming-leak",
  [ALARMING_LEAK_BATT]              = "alarming-leak-batt",
  [ALARMING_CARE]                   = "alarming-care",
  [ALARMING_CARE_BATT]              = "alarming-care-batt",
  [DOOR_CHIME]                      = "door-chime",
  [UNKNOWN_LED_MODE]                = "unknown"
};


/* Print command usage */
void usage(char *name) {
    int i;

    fprintf(stderr, "\nUsage: %s ledMode\n\n", name);
    fprintf(stderr, "\tPossible modes are:\n\n");
    for (i = BOOT_POST; i < UNKNOWN_LED_MODE; i++) {
        fprintf(stderr, "\t\t%s\n", state_names[i]);
    }
    fprintf(stderr, "\n");
    exit(1);
}


/* Handle LED settings based on mode pass in */
int main(int argc, char** argv)
{
    int i;
    char cmd[128];
    int duration = 0;
    FILE *f;

    /* Need to have at least one argument, the LED mode to set */
    if (argc < 2) {
        usage(argv[0]);
    }

    /* Strip \n off end of mode if there */
    if (argv[1][strlen(argv[1]) - 1] == '\n') {
        argv[1][strlen(argv[1]) - 1] = '\0';
    }

    /* Check for the mode */
    for (i = BOOT_POST; i < UNKNOWN_LED_MODE; i++) {
        if (strncmp(argv[1], state_names[i], MAX_STATE_NAME_LENGTH) == 0) {
	    break;
        }
    }

    /* Show usage if no match */
    if (i == UNKNOWN_LED_MODE) {
        fprintf(stderr, "\nUnknown Mode: %s\n", argv[1]);
        usage(argv[0]);
    }

    // Allow agent to pass in the duration as well to support setting the grace
    //  period for some modes. This value will trump any duration from the
    //  state array
    if (argc == 3) {
        duration = atoi(argv[2]);
    }

    /* Get last state */
    if (access(LAST_LED_MODE_FILE, F_OK) != -1) {
        f = fopen(LAST_LED_MODE_FILE, "r");
	if (f != NULL) {
	    fscanf(f, "%d", (int*)&lastMode);
	    fclose(f);
	}
    }

    /* When we are in some final states, don't allow someone (like the
       agent) to change the LEDs */
    if ((lastMode == TURNING_OFF) ||
        ((i != BOOT_LINUX) && (lastMode == SHUTDOWN)) ||
        ((i != SHUTDOWN) && (i != BUTTON_SOFT_RESET) &&
	 (lastMode == BUTTON_REBOOT)) ||
        ((i != SHUTDOWN) && (i != BUTTON_FACTORY_DEFAULT) &&
	 (lastMode == BUTTON_SOFT_RESET)) ||
        ((i != SHUTDOWN) && ((lastMode == BUTTON_FACTORY_DEFAULT) ||
                             (lastMode == FACTORY_RESET_ACK)))) {
        return 0;
    }

    /* Set LED state - use ledRing command to do so */
    if (states[i].mode != RING_OFF) {

        /* Kill any LED commands that may be running */
        snprintf(cmd, sizeof(cmd), "killall %s > /dev/null 2>&1", RING_APP);
        system(cmd);

        /* Override duration if not passed in */
        if (duration == 0) {
            duration = states[i].duration;
        }

        /* Record last mode */
        lastMode = i;
        f = fopen(LAST_LED_MODE_FILE, "w");
        if (f != NULL) {
            fprintf(f, "%d", (int)lastMode);
            fclose(f);
        }

        /* Set new LED ring mode */
        if (duration == 0) {
            snprintf(cmd, sizeof(cmd), "%s -c %X -m %d&", RING_APP,
                     states[i].color, states[i].mode);
        } else {
            snprintf(cmd, sizeof(cmd), "%s -c %X -m %d -d %d&", RING_APP,
                     states[i].color, states[i].mode, duration);
        }
        return system(cmd);
    }
    return 0;
}
