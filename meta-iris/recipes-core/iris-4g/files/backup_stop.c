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
#include <errno.h>
#include <syslog.h>
#include <irisdefs.h>
#include <at_parser.h>
#include "backup.h"

static void usage(char *name)
{
    fprintf(stderr, "\nusage: %s [options]\n"
            "  options:\n"
            "    -h              Print this message\n"
            "                    Stop backup modem connection\n"
            "\n", name);
}

/* Stop backup modem connection */
int main(int argc, char** argv)
{
    int  c;
    FILE *f;
    char cmd[256];
    char dongleID[128];
    int pipes[2];
    char *backup_if = NULL;

    /* Parse options... */
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

    // Make sure LTE modem is present
    f = fopen(LTE_DONGLE_FILE, "r");
    dongleID[0] = '\0';
    if (f) {
        fscanf(f, "%s", dongleID);
        fclose(f);
    }
    if (dongleID[0] == '\0') {
        syslog(LOG_ERR, "LTE modem is not present in system.\n");
        exit(1);
    }

    // What type of dongle is it?
    if (strncmp(dongleID, LTE_HUAWEI_MODEM_ID, strlen(LTE_HUAWEI_MODEM_ID)) == 0) {
        backup_if = BACKUP_ETH_IF;
    }
#ifdef ZTE_LTE_SUPPORTED
    else if (strncmp(dongleID, LTE_ZTEWELINK_MODEM_ID, strlen(LTE_ZTEWELINK_MODEM_ID)) == 0) {
        backup_if = BACKUP_USB_IF;

	// Need to tell the modem to disconnect
        ATPort *port = atOpen(LTE_ZTEWELINK_AT_PORT, LTE_ZTEWELINK_AT_SPEED,
                              LTE_ZTEWELINK_AT_FLOW);
        if (port == NULL) {
            syslog(LOG_ERR, "LTE modem - cannot open AT control port.\n");
            exit(1);
        }

        // Stop connection
        atSend(port, LTE_ZTEWELINK_AT_STOP, DEFAULT_AT_TIMEOUT);
        if (port->status != OK) {
            syslog(LOG_ERR, "LTE modem - error stoping call.\n");
            exit(1);
        }

        // Close port
        atClose(port);
    }
#endif

    // Redirect output so it doesn't show on console
    pipe(pipes);
    dup2(pipes[0], fileno(stderr));
    dup2(pipes[1], fileno(stdout));

    // Bring down interface
    if (backup_if != NULL) {
        snprintf(cmd, sizeof(cmd), "/sbin/ifdown %s", backup_if);
    }
#ifdef QUECTEL_LTE_SUPPORTED
    else {
        snprintf(cmd, sizeof(cmd), "/usr/bin/killall %s", LTE_QUECTEL_DAEMON);
    }
#endif
    system(cmd);

    return 0;
}
