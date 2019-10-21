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


// Syslog name
#define SYSLOG_IDENT               "dwatcher"

// Check delays
#define CHECK_PERIOD               5
#define INIT_DELAY                 60

// Daemons we care about
#define IRISINITD_PROCESS_NAME     "irisinitd"
#define BATTERYD_PROCESS_NAME      "batteryd"
#define IRIS4GD_PROCESS_NAME       "iris4gd"
#define IRISNFCD_PROCESS_NAME      "irisnfcd"
#define IRIS4GD_PATH               "/usr/bin/iris4gd"
#define IRISNFCD_PATH              "/usr/bin/irisnfcd"


static int checkProcess(char *process)
{
    FILE *f;
    char cmd[256];
    char line[128];
    int  res = 0;

    snprintf(cmd, sizeof(cmd), "pidof %s", process);
    f = popen(cmd, "r");
    if (f) {
        // Is process running?
        if (fgets(line, sizeof(line), f) != NULL) {
            res = 1;
        }
        pclose(f);
    }
    return res;
}


// Simple watcher to see if any of our daemons are no longer running
int main(int argc, char** argv)
{
    char cmd[128];

    // Turn ourselves into a daemon
    daemon(1, 1);

    // Setup syslog
    openlog(SYSLOG_IDENT, (LOG_CONS | LOG_PID | LOG_PERROR), LOG_DAEMON);

    // Wait for the system to settle before checking
    sleep(INIT_DELAY);

    // Checked forever...
    while (1) {

        // Sleep between checks
        sleep(CHECK_PERIOD);

        // Make sure our init daemon is running!
        if (!checkProcess(IRISINITD_PROCESS_NAME)) {
            syslog(LOG_INFO, "Restarting Iris init daemon...");
            snprintf(cmd, sizeof(cmd), "%s restart", IRISINITD_PROCESS_NAME);
            system(cmd);
        }

        // For release image, check for other daemons...
        if (IRIS_isReleaseImage()) {

            // Battery daemon
            if (!checkProcess(BATTERYD_PROCESS_NAME)) {
                syslog(LOG_INFO, "Restarting battery daemon...");
                system(BATTERYD_PROCESS_NAME);
            }

            // 4G daemon
            if ((access(IRIS4GD_PATH, F_OK) != -1) &&
		!checkProcess(IRIS4GD_PROCESS_NAME)) {
                syslog(LOG_INFO, "Restarting 4G daemon...");
                system(IRIS4GD_PROCESS_NAME);
            }

#ifdef imxdimagic
#ifdef LATER
            // NFC daemon - not supported yet...
            if ((access(IRISNFCD_PATH, F_OK) != -1) &&
		!checkProcess(IRISNFCD_PROCESS_NAME)) {
                syslog(LOG_INFO, "Restarting NFC daemon...");
                system(IRISNFCD_PROCESS_NAME);
            }
#endif
#endif
        }
    }

    // Should never get here...
    return 0;
}
