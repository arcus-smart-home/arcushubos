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

/* Possible LED settings - off, on, blink@0.5Hz, blink@1Hz and blink@2Hz */
typedef enum {
  LED_OFF,
  LED_ON,
  LED_0_5HZ,
  LED_1HZ,
  LED_2HZ
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
  UNKNOWN_LED_MODE
} leds_mode_t;


/* Constants */
#define NUM_LEDS                 3
#define MAX_STATE_NAME_LENGTH    24
#define DELAY_ON_1HZ             500
#define DELAY_OFF_1HZ            500
#define DELAY_ON_2HZ             250
#define DELAY_OFF_2HZ            250
#define DELAY_ON_0_5HZ           1000
#define DELAY_OFF_0_5HZ          1000
#define BRIGHTNESS_OFF           0
#define BRIGHTNESS_ON_FULL       255

/* Sysfs file paths for LEDs */
#define SYSFS_LED_GREEN_BRIGHTNESS  "/sys/class/leds/green/brightness"
#define SYSFS_LED_GREEN_TRIGGER     "/sys/class/leds/green/trigger"
#define SYSFS_LED_GREEN_DELAY_ON    "/sys/class/leds/green/delay_on"
#define SYSFS_LED_GREEN_DELAY_OFF   "/sys/class/leds/green/delay_off"
#define SYSFS_LED_YELLOW_BRIGHTNESS "/sys/class/leds/yellow/brightness"
#define SYSFS_LED_YELLOW_TRIGGER    "/sys/class/leds/yellow/trigger"
#define SYSFS_LED_YELLOW_DELAY_ON   "/sys/class/leds/yellow/delay_on"
#define SYSFS_LED_YELLOW_DELAY_OFF  "/sys/class/leds/yellow/delay_off"
#define SYSFS_LED_RED_BRIGHTNESS    "/sys/class/leds/red/brightness"
#define SYSFS_LED_RED_TRIGGER       "/sys/class/leds/red/trigger"
#define SYSFS_LED_RED_DELAY_ON      "/sys/class/leds/red/delay_on"
#define SYSFS_LED_RED_DELAY_OFF     "/sys/class/leds/red/delay_off"

/* State of all LEDs */
typedef struct led_state {
  led_setting_t green;
  led_setting_t yellow;
  led_setting_t red;
} led_state_t;


/* LED states - from software requirements */
static led_state_t states[] = {
  [BOOT_POST]                       = { LED_1HZ, LED_1HZ, LED_1HZ },
  [BOOT_LINUX]                      = { LED_1HZ, LED_1HZ, LED_OFF },
  [BOOT_AGENT]                      = { LED_1HZ, LED_OFF, LED_OFF },
  [NORMAL_UNASSOC_CONN_PRI]         = { LED_0_5HZ, LED_OFF, LED_OFF },
  [NORMAL_UNASSOC_CONN_PRI_BATT]    = { LED_0_5HZ, LED_0_5HZ, LED_OFF },
  [NORMAL_UNASSOC_CONN_BACKUP]      = { LED_0_5HZ, LED_ON, LED_OFF },
  [NORMAL_UNASSOC_CONN_BACKUP_BATT] = { LED_0_5HZ, LED_0_5HZ, LED_OFF },
  [NORMAL_UNASSOC_DISCONN]          = { LED_0_5HZ, LED_OFF, LED_1HZ },
  [NORMAL_UNASSOC_DISCONN_BATT]     = { LED_0_5HZ, LED_0_5HZ, LED_1HZ },
  [NORMAL_ASSOC_CONN_PRI]           = { LED_ON, LED_OFF, LED_OFF },
  [NORMAL_ASSOC_CONN_PRI_BATT]      = { LED_ON, LED_0_5HZ, LED_OFF },
  [NORMAL_ASSOC_CONN_BACKUP]        = { LED_ON, LED_ON, LED_OFF },
  [NORMAL_ASSOC_CONN_BACKUP_BATT]   = { LED_ON, LED_0_5HZ, LED_OFF },
  [NORMAL_ASSOC_DISCONN]            = { LED_ON, LED_OFF, LED_1HZ },
  [NORMAL_ASSOC_DISCONN_BATT]       = { LED_ON, LED_0_5HZ, LED_1HZ },
  [NORMAL_PAIRING_CONN_PRI]         = { LED_1HZ, LED_OFF, LED_OFF },
  [NORMAL_PAIRING_CONN_PRI_BATT]    = { LED_1HZ, LED_0_5HZ, LED_OFF },
  [NORMAL_PAIRING_CONN_BACKUP]      = { LED_1HZ, LED_ON, LED_OFF },
  [NORMAL_PAIRING_CONN_BACKUP_BATT] = { LED_1HZ, LED_0_5HZ, LED_OFF },
  [NORMAL_PAIRING_DISCONN]          = { LED_1HZ, LED_OFF, LED_1HZ },
  [NORMAL_PAIRING_DISCONN_BATT]     = { LED_1HZ, LED_0_5HZ, LED_1HZ },
  [UPGRADE_DECRYPT]                 = { LED_OFF, LED_2HZ, LED_OFF },
  [UPGRADE_DECRYPT_ERR]             = { LED_OFF, LED_2HZ, LED_ON },
  [UPGRADE_UNPACK]                  = { LED_OFF, LED_2HZ, LED_OFF },
  [UPGRADE_UNPACK_ERR]              = { LED_OFF, LED_2HZ, LED_ON },
  [UPGRADE_BOOTLOADER]              = { LED_OFF, LED_OFF, LED_2HZ },
  [UPGRADE_BOOTLOADER_ERR]          = { LED_OFF, LED_OFF, LED_ON },
  [UPGRADE_KERNEL]                  = { LED_OFF, LED_OFF, LED_2HZ },
  [UPGRADE_KERNEL_ERR]              = { LED_OFF, LED_1HZ, LED_ON },
  [UPGRADE_ROOTFS]                  = { LED_OFF, LED_OFF, LED_2HZ },
  [UPGRADE_ROOTFS_ERR]              = { LED_1HZ, LED_1HZ, LED_ON },
  [UPGRADE_ZIGBEE]                  = { LED_2HZ, LED_OFF, LED_OFF },
  [UPGRADE_ZIGBEE_ERR]              = { LED_2HZ, LED_OFF, LED_ON },
  [UPGRADE_ZWAVE]                   = { LED_2HZ, LED_OFF, LED_OFF },
  [UPGRADE_ZWAVE_ERR]               = { LED_2HZ, LED_OFF, LED_ON },
  [UPGRADE_BTE]                     = { LED_2HZ, LED_OFF, LED_OFF },
  [UPGRADE_BTE_ERR]                 = { LED_2HZ, LED_OFF, LED_ON },
  [SHUTDOWN]                        = { LED_ON, LED_ON, LED_ON },
  [BUTTON_REBOOT]                   = { LED_OFF, LED_OFF, LED_ON },
  [BUTTON_SOFT_RESET]               = { LED_OFF, LED_ON, LED_ON },
  [BUTTON_FACTORY_DEFAULT]          = { LED_ON, LED_ON, LED_ON },
  [ALL_ON]                          = { LED_ON, LED_ON, LED_ON },
  [ALL_OFF]                         = { LED_OFF, LED_OFF, LED_OFF },
  [UNKNOWN_LED_MODE]                = { LED_OFF, LED_OFF, LED_OFF}
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
    int i, j;

    /* Need to have a single argument, the LED mode to set */
    if (argc != 2) {
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

    /* Set LED state for all */
    for (j = 0; j < NUM_LEDS; j++) {
        char *sysfs_brightness, *sysfs_trigger, *sysfs_delay_on,
             *sysfs_delay_off;
        led_setting_t led_mode;
        FILE *f_brightness, *f_trigger, *f_delay_on, *f_delay_off;
        int  brightness, timer, delay_on, delay_off;

        /* Reset files */
        f_brightness = f_trigger = f_delay_on = f_delay_off = NULL;

        /* Get LED mode and sysfs file names */
        if (j == 0) {
            led_mode = states[i].green;
            sysfs_brightness = SYSFS_LED_GREEN_BRIGHTNESS;
            sysfs_trigger = SYSFS_LED_GREEN_TRIGGER;
            sysfs_delay_on = SYSFS_LED_GREEN_DELAY_ON;
            sysfs_delay_off = SYSFS_LED_GREEN_DELAY_OFF;
        } else if (j == 1) {
            led_mode = states[i].yellow;
            sysfs_brightness = SYSFS_LED_YELLOW_BRIGHTNESS;
            sysfs_trigger = SYSFS_LED_YELLOW_TRIGGER;
            sysfs_delay_on = SYSFS_LED_YELLOW_DELAY_ON;
            sysfs_delay_off = SYSFS_LED_YELLOW_DELAY_OFF;
        } else {
            led_mode = states[i].red;
            sysfs_brightness = SYSFS_LED_RED_BRIGHTNESS;
            sysfs_trigger = SYSFS_LED_RED_TRIGGER;
            sysfs_delay_on = SYSFS_LED_RED_DELAY_ON;
            sysfs_delay_off = SYSFS_LED_RED_DELAY_OFF;
        }

        /* We always need the brightness and trigger files */
        f_brightness = fopen(sysfs_brightness, "w");
        f_trigger = fopen(sysfs_trigger, "w");

        /* Start with LED off to avoid odd blink patterns */
        if (f_brightness) {
            fprintf(f_brightness, "%d\n", BRIGHTNESS_OFF);
            fflush(f_brightness);
            rewind(f_brightness);
        }

        /* Switch on LED state */
        switch (led_mode) {

        default:
        case LED_OFF:
            /* Nothing to do - was just turned off above */
            if (f_brightness) {
                fclose(f_brightness);
            }
            if (f_trigger) {
                fclose(f_trigger);
            }
            continue;
            break;

        case LED_ON:
            brightness = BRIGHTNESS_ON_FULL;
            timer = 0;
            break;

        case LED_0_5HZ:
            brightness = BRIGHTNESS_ON_FULL;
            timer = 1;
            delay_on = DELAY_ON_0_5HZ;
            delay_off = DELAY_OFF_0_5HZ;
            break;

        case LED_1HZ:
            brightness = BRIGHTNESS_ON_FULL;
            timer = 1;
            delay_on = DELAY_ON_1HZ;
            delay_off = DELAY_OFF_1HZ;
            break;

        case LED_2HZ:
            brightness = BRIGHTNESS_ON_FULL;
            timer = 1;
            delay_on = DELAY_ON_2HZ;
            delay_off = DELAY_OFF_2HZ;
            break;
        }

        /* Update LED settings */
        if (f_brightness) {
            fprintf(f_brightness, "%d\n", brightness);
            fclose(f_brightness);
        }
        if (f_trigger) {
            if (timer) {
                fprintf(f_trigger, "timer\n");
            } else {
                fprintf(f_trigger, "default-on\n");
            }
            fclose(f_trigger);
        }
        if (timer) {
            f_delay_on = fopen(sysfs_delay_on, "w");
            f_delay_off = fopen(sysfs_delay_off, "w");
            if (f_delay_on) {
                fprintf(f_delay_on, "%d\n", delay_on);
                fclose(f_delay_on);
            }
            if (f_delay_off) {
                fprintf(f_delay_off, "%d\n", delay_off);
                fclose(f_delay_off);
            }
        }
    }
    return 0;
}
