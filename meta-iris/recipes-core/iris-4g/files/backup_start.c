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
#include <irislib.h>
#include <at_parser.h>
#include "backup.h"

#define TIMEOUT_PATH     "/usr/bin/timeout"
// Just try once now - the agent will retry if necessary
#define MAX_ATTEMPTS     1
#define UP_TIMEOUT       "10"

static void usage(char *name)
{
    fprintf(stderr, "\nusage: %s [options]\n"
            "  options:\n"
            "    -h              Print this message\n"
            "                    Start backup LTE modem connection\n"
            "\n", name);
}

/* Start backup modem connection */
int main(int argc, char** argv)
{
    int  c, count = 0;
    FILE *f;
    char cmd[256];
    char result[128];
    int pipes[2];
#ifdef QUECTEL_LTE_SUPPORTED
    int qmi_dongle = 0;
#endif
#ifdef ZTE_LTE_SUPPORTED
    int at_dongle = 0;
#endif
    char backup_if[16];

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
    result[0] = '\0';
    if (f) {
        fscanf(f, "%s", result);
        fclose(f);
    }
    if (result[0] == '\0') {
        syslog(LOG_ERR, "LTE modem is not present in system.\n");
        exit(1);
    }

    // What type of dongle is it?
    if (strncmp(result, LTE_HUAWEI_MODEM_ID, strlen(LTE_HUAWEI_MODEM_ID)) == 0) {
        char *intf;

#ifdef QUECTEL_LTE_SUPPORTED
        qmi_dongle = 0;
#endif
#ifdef ZTE_LTE_SUPPORTED
        at_dongle = 0;
#endif
        snprintf(backup_if, sizeof(backup_if), "%s", BACKUP_ETH_IF);

        // If Wifi is up, use over Ethernet
        if (IRIS_isIntfIPUp(PRIMARY_WLAN_IF)) {
            intf = PRIMARY_WLAN_IF;
        } else {
            intf = PRIMARY_ETH_IF;
        }

        // Since dongle has fixed address, don't allow it to start
        //  if its subnet overlaps with primary!
        snprintf(cmd, sizeof(cmd), "/sbin/ip addr show %s | grep %s",
                 intf, LTE_HUAWEI_MODEM_SUBNET);
        result[0] = '\0';
        f = popen(cmd, "r");
        if (f) {
            fscanf(f, "%s", result);
        }
        pclose(f);
        if (result[0] != '\0') {
            syslog(LOG_ERR,
                   "LTE modem cannot be started due to Ethernet configuration!\n");
            exit(1);
        }
#ifdef QUECTEL_LTE_SUPPORTED
    } else if (strncmp(result, LTE_QUECTEL_MODEM_ID,
                       strlen(LTE_QUECTEL_MODEM_ID)) == 0) {
        qmi_dongle = 1;
        snprintf(backup_if, sizeof(backup_if), "%s", BACKUP_WWAN_IF);
#endif
#ifdef ZTE_LTE_SUPPORTED
    } else if (strncmp(result, LTE_ZTEWELINK_MODEM_ID,
                       strlen(LTE_ZTEWELINK_MODEM_ID)) == 0) {
        at_dongle = 1;
        snprintf(backup_if, sizeof(backup_if), "%s", BACKUP_USB_IF);
#endif
    } else {
        syslog(LOG_ERR, "LTE modem model is not supported.\n");
        exit(1);
    }

    /* If backup connection is already in use, exit*/
    snprintf(cmd, sizeof(cmd), BACKUP_CHECK_IF_STATUS, backup_if);
    f = popen(cmd, "r");
    if (f) {
        char line[256];
        if (fgets(line, sizeof(line), f) != NULL) {
            pclose(f);
            return 0;
        }
        pclose(f);
    }

    // Setup depends on type...
#ifdef QUECTEL_LTE_SUPPORTED
    if (qmi_dongle) {
        snprintf(cmd, sizeof(cmd), "/usr/bin/killall %s", LTE_QUECTEL_DAEMON);
    } else
#endif
    {
        // Make sure an old version of the DHCP client isn't already running
        snprintf(cmd, sizeof(cmd),
                 "ps | grep %s | grep %s | awk {'print $1'} | xargs kill -9",
                 DHCP_CLIENT, backup_if);
        system(cmd);
    }

    // Redirect output so it doesn't show on console
    pipe(pipes);
    dup2(pipes[0], fileno(stderr));
    dup2(pipes[1], fileno(stdout));

#ifdef QUECTEL_LTE_SUPPORTED
    // When using connection manager, it handles retries...
    if (qmi_dongle) {
        if (execl(TIMEOUT_PATH, "timeout", "-t", UP_TIMEOUT, "-s", "9",
                  "sh", "-c", BACKUP_WWAN_START_CMD, (char *)NULL)) {
            return 1;
        } else {
            return 0;
        }
    }
#endif
#ifdef ZTE_LTE_SUPPORTED
    if (at_dongle) {
        // Send AT commands needed to connect
        ATPort *port = atOpen(LTE_ZTEWELINK_AT_PORT, LTE_ZTEWELINK_AT_SPEED,
                              LTE_ZTEWELINK_AT_FLOW);
        if (port == NULL) {
            syslog(LOG_ERR, "LTE modem - cannot open AT control port.\n");
            exit(1);
        }

        // Send setup command
        atSend(port, LTE_ZTEWELINK_AT_SETUP, DEFAULT_AT_TIMEOUT);
        if (port->status != OK) {
            syslog(LOG_ERR, "LTE modem - error during call setup.\n");
            exit(1);
        }

        // Start connection
        atSend(port, LTE_ZTEWELINK_AT_START, CONNECT_AT_TIMEOUT);
        if (port->status != OK) {
            syslog(LOG_ERR, "LTE modem - error starting call.\n");
            exit(1);
        }

        // Close port
        atClose(port);
    }
#endif

    // Try a few times if interface fails to come up
    while (count < MAX_ATTEMPTS) {
        // Bring interface down to start
        snprintf(cmd, sizeof(cmd), "/sbin/ifconfig %s down", backup_if);
        system(cmd);
        snprintf(cmd, sizeof(cmd), "/sbin/ifdown %s", backup_if);
        system(cmd);

        // Start up backup interface - DHCP may not work, so give up if
        //  we aren't successful in a short period of time
        snprintf(cmd, sizeof(cmd), "/sbin/ifconfig %s up", backup_if);
        system(cmd);

        // Force MTU to a set value to work around dongle issue
        snprintf(cmd, sizeof(cmd), "/sbin/ifconfig %s mtu %s",
                 backup_if, BACKUP_MTU);
        system(cmd);

        // Create start command
        snprintf(cmd, sizeof(cmd), "%s %s", BACKUP_START_CMD, backup_if);
        if (execl(TIMEOUT_PATH, "timeout", "-t", UP_TIMEOUT, "-s", "9",
                  "sh", "-c", cmd, (char *)NULL)) {

            // Kill dhcp client - doesn't get killed by the timeout
            snprintf(cmd, sizeof(cmd),
                   "ps | grep %s | grep %s | awk {'print $1'} | xargs kill -9",
                    DHCP_CLIENT, backup_if);
            system(cmd);

            // Up retry count
            count++;
        } else {
            return 0;
        }
    }

    fprintf(stderr, "Failed to bring up backup interface!\n");
    exit(1);
}

