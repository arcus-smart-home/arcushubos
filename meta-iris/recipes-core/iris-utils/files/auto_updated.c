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
#include <irislib.h>
#include "build_image.h"


/* Wait a bit before initial check, and between failures */
#define INITIAL_DELAY   (60 * 3)
#define MAX_RETRIES     5
#define RETRY_DELAY     (60 * 5)

/* Check every hour - probably want to check every day after initial release! */
#define PERIODIC_DELAY  (60 * 60)

#define SYSLOG_IDENT       "updated"
#define UPDATE_TMP_FILE    "/tmp/auto_update.bin"


/* Auto firmware update daemon - check periodically for an update and
    install and reboot if new code exists */
int main(int argc, char** argv)
{
    /* Turn ourselves into a daemon */
    daemon(1, 1);

    /* Setup syslog */
    openlog(SYSLOG_IDENT, (LOG_CONS | LOG_PID | LOG_PERROR), LOG_DAEMON);

    /* Sleep for a bit to let system complete startup */
    sleep(INITIAL_DELAY);

    /* Firmware check loop */
    while (1) {
        int   count, i;
        FILE  *f;
        char  cmd[256];
        int   res;

        /* Do we want to skip updating?  This is a developer feature */
        if (access(UPDATE_SKIP_FILE, F_OK) != -1) {
            goto try_again;
        }

        /* Create curl command to read update file */
        snprintf(cmd, sizeof(cmd), "curl -s -k -u \"%s:%s\" %s%s",
                 UPDATE_USERNAME, UPDATE_PASSWORD,
                 UPDATE_SERVER_URL, UPDATE_VER_FILE);

        /* Get update file */
        syslog(LOG_INFO, "Checking for firmware update");
        for (count = 0; count < MAX_RETRIES; count++) {
            f = popen(cmd, "r");
            if (f != NULL) {
                break;
            }
            syslog(LOG_INFO, "Update check failed - retrying in %d seconds",
                   RETRY_DELAY);
            pclose(f);

            /* Wait a few minutes before trying again ... */
            sleep(RETRY_DELAY);
        }

        /* Did we get a file? */
        if (count < MAX_RETRIES) {
            char  filename[128], version[128];
            int   major, minor, point, build;
            char  type[32], suffix[32];
            char ledMode[128] = { 0 };
            int  ledDelay = 3;

            /* Get file */
            fgets(filename, sizeof(filename), f);

            /* Check for proper format - must start with hubOS */
            if (strncmp(filename, FIRMWARE_PREFIX, sizeof(FIRMWARE_PREFIX)-1)) {
                /* Bad filename - try again */
                syslog(LOG_INFO,
                       "Invalid update filename (%s)- will retry later",
                       filename);
                goto try_again;
            }

            syslog(LOG_INFO, "Firmware update file: %s", filename);
            /* Removing trailing \n */
            if (filename[strlen(filename)-1] == '\n') {
                filename[strlen(filename)-1] = '\0';
            }
            pclose(f);

            /* Check version */
            strncpy(version, filename, sizeof(version));

            /* Replace _ and . with spaces to make parsing easier */
            for (i = 0; i < strlen(version); i++) {
                char p = version[i];
                if ((p == '_') || (p == '.')) {
                    version[i] = ' ';
                }
            }
            /* Catch case when type is missing (release image) */
            if (isdigit(version[6])) {
                sscanf(version, "hubOS %d %d %d %d%s bin",
                       &major, &minor, &point, &build, &suffix);
            } else {
                sscanf(version, "hubOS %s %d %d %d %d%s bin", &type,
                       &major, &minor, &point, &build, &suffix);
            }
            /* If suffix was blank, we'll pick up the "bin" at the end ... */
            if (strncmp("bin", suffix, sizeof(suffix)) == 0) {
                suffix[0] = '\0';
            }

            /* If version is the same as current build, skip install */
            if ((major == VERSION_MAJOR) && (minor == VERSION_MINOR) &&
                (point == VERSION_POINT) && (build == VERSION_BUILD) &&
                (strncmp(VERSION_SUFFIX, suffix, sizeof(suffix)) == 0)) {

                syslog(LOG_INFO, "Currently running the latest firmware "
                       "- will check again later");
                goto try_again;
            }

            /* If different, get file */
            remove(UPDATE_TMP_FILE);
            snprintf(cmd, sizeof(cmd), "curl -s -k -u \"%s:%s\" %s%s -o %s",
                     UPDATE_USERNAME, UPDATE_PASSWORD,
                     UPDATE_SERVER_URL, filename, UPDATE_TMP_FILE);

            syslog(LOG_INFO, "Downloading firmware update...");
            for (count = 0; count < MAX_RETRIES; count++) {
                res = system(cmd);

                /* Does output file exist? */
                if (!res && (access(UPDATE_TMP_FILE, F_OK) != -1)) {
                    break;
                }
                syslog(LOG_INFO, "Download failed - retrying in %d seconds",
                       RETRY_DELAY);

                /* Wait a few minutes before trying again ... */
                sleep(RETRY_DELAY);
            }

            /* Get current LED mode so we can restore later */
            IRIS_getLedMode(ledMode, sizeof(ledMode));

            /* If we got file, validate and install it */
            if (count < MAX_RETRIES) {
                syslog(LOG_INFO, "Validating firmware update...");
                snprintf(cmd, sizeof(cmd), "validate_image -s %s",
                         UPDATE_TMP_FILE);
                res = system(cmd);

                /* If validation passes, install */
                if (!res && (access(FW_INSTALL_FILE, F_OK) != -1)) {
                    syslog(LOG_INFO, "Installing firmware update...");
                    snprintf(cmd, sizeof(cmd), "fwinstall %s", FW_INSTALL_FILE);
                    res = system(cmd);

                    /* Reboot if install was successful */
                    if (!res) {
                        syslog(LOG_INFO,
                               "Firmware install was successful - rebooting");
                        system("hub_restart now");
                        exit(0);
                    } else {
                        /* Hard error? - leave LEDs on for 10 seconds until
                           reboot */
                        if (res >= INSTALL_HARD_ERRORS) {
                            ledDelay = 10;
                        }
                        syslog(LOG_ERR, "Firmware installation failed!");
                    }
                } else {
                    syslog(LOG_ERR, "Firmware validation failed!");
                }

                /* Restore LEDs */
                if (res) {
                    /* Wait up to 10 seconds for flashing to stop then restore
                       - we may have rebooted on a hard error! */
                    sleep(ledDelay);
                }
                IRIS_setLedMode(ledMode);
            }

            /* Clean up temp files */
            remove(UPDATE_TMP_FILE);
            remove(FW_INSTALL_FILE);
        }
        syslog(LOG_INFO, "Update check failed - will retry in %d seconds",
               PERIODIC_DELAY);

    try_again:
        /* Try again in a while ... */
        sleep(PERIODIC_DELAY);
    }

    /* Should never get here ... */
    exit(0);
}


