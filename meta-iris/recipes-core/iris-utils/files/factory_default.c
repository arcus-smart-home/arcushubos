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

#define _XOPEN_SOURCE 500
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ftw.h>
#include <sys/reboot.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <signal.h>
#include <irislib.h>

// Reformat rather than deleting directories as it is faster
#define REFORMAT_DATA_PARTITION  1

// Defines for reformat
#define TMP_FIRMWARE_DIR    "/tmp/firmware/"
#define DATA_FIRMWARE_DIR   "/data/firmware/"
#define REFORMAT_STR        "mkfs.ext4 -q -F -E stripe-width=4096 -L data /dev/data"
#define UMOUNT_RETRIES      50


static void usage(char *name)
{
    fprintf(stderr, "\nusage: %s [options]\n"
            "  options:\n"
            "    -h              Print this message\n"
            "                    Perform hub factory default\n"
            "\n", name);
}

#ifndef REFORMAT_DATA_PARTITION
// File remove callback
static int file_remove_cb(const char *fpath, const struct stat *sb,
                          int typeflag, struct FTW *ftwbuf)
{
    return remove(fpath);
}
#endif

// Terminate a program
static int terminate_program(char *program)
{
    char   cmd[128];

    // Use killall to remove all instances
    snprintf(cmd, sizeof(cmd), "killall %s", program);
    return system(cmd);
}


// Perform factory default
int main(int argc, char** argv)
{
    int  c;

    // Parse options...
    opterr = 0;
    while ((c = getopt(argc, argv, "h")) != -1)
    switch (c) {
    case 'h':
        usage(argv[0]);
        return 0;
    case '?':
        fprintf(stderr, "Unknown option `\\x%x'.\n", optopt);
        usage(argv[0]);
        exit(1);
    default:
        fprintf(stderr, "Error parsing options - exiting!\n");
        usage(argv[0]);
        exit(1);
    }

    fprintf(stderr, "Performing factory default of hub!\n");

#ifdef imxdimagic
        // Stay in shutdown mode so agent can't change LED ring
        IRIS_setLedMode("shutdown");
#endif

#ifndef REFORMAT_DATA_PARTITION
    // Remove configuration files
    fprintf(stderr, "Clearing hub configuration...\n");
    if (nftw(DATA_CONFIG_DIR, file_remove_cb, 64, FTW_DEPTH | FTW_PHYS) != 0) {
        fprintf(stderr, "Error removing Hub configuration: %s\n",
                strerror(errno));
    }

    // Remove log files
    fprintf(stderr, "Clearing hub log files...\n");
    if (nftw(DATA_LOG_DIR, file_remove_cb, 64, FTW_DEPTH | FTW_PHYS) != 0) {
        fprintf(stderr, "Error removing Hub persistent logs: %s\n",
                strerror(errno));
    }
#endif

    // Clean up agent if release image
    if (IRIS_isReleaseImage()) {

        // Kill off irisagentd so we don't attempt to restart the Hub agent
        if (terminate_program("irisagentd")) {
            fprintf(stderr, "Unable to terminate Hub Agent watchdog!\n");
        } else {
            fprintf(stderr, "Hub Agent watchdog has been terminated.\n");
        }
        pokeWatchdog();

        // Kill off Hub agent itself
        if (terminate_program("java")) {
            fprintf(stderr, "Unable to terminate Hub Agent!\n");
        } else {
            fprintf(stderr, "Hub Agent has been terminated.\n");
        }
        pokeWatchdog();

#ifdef imxdimagic
        // Turn on boot LEDs so ring isn't dark!
        IRIS_setLedMode("boot-linux");
#endif
        // Wait a moment to make sure serial ports are released
        sleep(10);
        pokeWatchdog();

#ifndef REFORMAT_DATA_PARTITION
        // Clear agent data
        fprintf(stderr, "Clearing hub agent...\n");
        if (nftw(AGENT_ROOT_DIR, file_remove_cb, 64,
                 FTW_DEPTH | FTW_PHYS) != 0) {
            fprintf(stderr, "Error removing hub agent: %s\n", strerror(errno));
        }
        pokeWatchdog();

        // Clear jre libraries
        fprintf(stderr, "Clearing Java libraries...\n");
        if (nftw(DATA_JRE_LIBS_DIR, file_remove_cb, 64,
                 FTW_DEPTH | FTW_PHYS) != 0) {
            fprintf(stderr, "Error removing java libs: %s\n", strerror(errno));
        }
        pokeWatchdog();

        // Clear agent logs and database
        fprintf(stderr, "Clearing hub agent data...\n");
        if (nftw(DATA_IRIS_DIR, file_remove_cb, 64,
                 FTW_DEPTH | FTW_PHYS) != 0) {
            fprintf(stderr, "Error removing hub agent data: %s\n",
                    strerror(errno));
        }
        pokeWatchdog();
#endif
    }

#ifdef REFORMAT_DATA_PARTITION
    // Reformat entire /data partition as it is faster than deleting
    //  now that discard support is enabled
    {
        char cmd[128];
        int retries = 0;

        fprintf(stderr, "Reformating data partition...\n");

        // Copy firmware data to avoid unnecessary radio re-installs
        snprintf(cmd, sizeof(cmd), "mkdir -p %s; cp %s* %s", TMP_FIRMWARE_DIR,
                 DATA_FIRMWARE_DIR, TMP_FIRMWARE_DIR);
        if (system(cmd)) {
            fprintf(stderr, "Unable to relocate firmware files!\n");
        }
        pokeWatchdog();

        // Kill all other processes to make sure nothing is touching /data
        while (retries++ < UMOUNT_RETRIES) {
            snprintf(cmd, sizeof(cmd), "killall5 -9");
            if (system(cmd)) {
                fprintf(stderr, "Unable to terminate all other processes!\n");
            }
            sleep(5);
            pokeWatchdog();

            // Unmount /data
            if (!umount("/data")) {
                // Reformat /data
                if (system(REFORMAT_STR)) {
                    fprintf(stderr, "Unable to reformat data partition!\n");
                    continue;
                }
                pokeWatchdog();

                // Remount and restore firmware data
                snprintf(cmd, sizeof(cmd),"mount /data; mkdir -p %s; cp %s* %s",
                         DATA_FIRMWARE_DIR, TMP_FIRMWARE_DIR,
                         DATA_FIRMWARE_DIR);
                if (system(cmd)) {
                    fprintf(stderr, "Unable to restore firmware files!\n");
                }
                pokeWatchdog();
                break;
            } else {
                // Don't give up easily!
                if (retries < UMOUNT_RETRIES) {
                    fprintf(stderr, "Retry /data umount!\n");
                } else {
                    fprintf(stderr,
                            "Unable to unmount and reformat data partition!\n");
                }
            }
        }
    }
#endif

    // Factory default Zwave module
    if (system("zwave_default")) {
        fprintf(stderr, "Unable to factory default Zwave module!\n");
    }
    pokeWatchdog();

    // Factory default Zigbee module
    if (system("zigbee_default")) {
        fprintf(stderr, "Unable to factory default Zigbee module!\n");
    }
    pokeWatchdog();

    // Reboot
    fprintf(stderr, "Rebooting hub...\n");
    sync(); sync();
    if (reboot(RB_AUTOBOOT)) {
        fprintf(stderr, "Error attempting factory default: %s\n",
                strerror(errno));
        exit(1);
    }
    return 0;
}

