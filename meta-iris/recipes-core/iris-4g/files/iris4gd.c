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
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <sys/inotify.h>
#include <pthread.h>
#include <at_parser.h>
#include <irislib.h>
#include "backup.h"


#define SYSLOG_IDENT          "iris4gd"
#define GATEWAY_CHECK_DELAY   5
#define MODULE_REBOOT_DELAY   15
#define AT_STATUS_CHECK_DELAY 15
#define AT_STATUS_INIT_DELAY  30
#define AT_STATUS_RESET_DELAY 10

static volatile int done = 0;

static void lteModuleReset(void)
{
    char cmd[128];

    /* Reset LTE module via GPIO, if available */
    if (access(LTE_RESET_GPIO_VALUE_FILE, F_OK) == 0) {
        printf("Resetting LTE module\n");
        snprintf(cmd, sizeof(cmd), "echo 1 > %s", LTE_RESET_GPIO_VALUE_FILE);
        system(cmd);
        usleep(10*1000);
        snprintf(cmd, sizeof(cmd), "echo 0 > %s", LTE_RESET_GPIO_VALUE_FILE);
        system(cmd);
        sleep(1);
    }
}

static void iris4g_sig_handler(int sig)
{
    switch (sig) {
    case SIGHUP:
#ifdef imxdimagic
        // No need to stop on hangup, will already be gone
        break;
#endif
    case SIGTERM:
        // Bring down eth1 before we die
        fprintf(stderr, "Stopping LTE connection...\n");
        system(LTE_STOP_COMMAND);
        done = 1;
        exit(0);
        break;
    }
}

/* Simple LTE control handler */
static void *lteControlThreadHandler(void *ptr)
{
    FILE *f;
    int  fd;
    char buf[512];
    int  len;

    /* We want to be notified when the LED mode file is changed */
    fd = inotify_init();
    if (fd >= 0) {

        /* Add LTE control file to watch list */
        inotify_add_watch(fd, LTE_CONTROL_FILE, IN_MODIFY);

        /* Keep looking for file modifications... */
        while (1) {

            /* Read inotify fd to get events */
            len = read(fd, buf, sizeof(buf));
            if (len >= 0) {
                int i;

                /* Parse events, looking for modify events ... */
                i = 0;
                while (i < len) {
                    struct inotify_event *event =
		      (struct inotify_event *)&buf[i];

                    /* Got a file modification event */
                    if (event->mask & IN_MODIFY) {

                        /* Open lteControl file */
                        f = fopen(LTE_CONTROL_FILE, "r");
                        if (f != NULL) {
                            char data[128];

                            /* Read data from lteControl file */
                            memset(data, 0, sizeof(data));
                            if (fread(data, 1, sizeof(data)-1, f) <= 0) {
                                /* Try again */
                                i += (sizeof(struct inotify_event)) +
                                  event->len;
				fclose(f);
                                continue;
                            }

                            /* Removing trailing \n */
                            if (data[strlen(data) - 1] == '\n') {
                                data[strlen(data) - 1] = '\0';
                            }

                            /* Connect or disconnect */
			    if (strncmp(data, "connect", 7) == 0) {
			        system(LTE_START_COMMAND);
			    } else if (strncmp(data, "disconnect", 10) == 0) {
				system(LTE_STOP_COMMAND);
			    } else if (strncmp(data, "init", 4) == 0) {
			        system(LTE_INIT_COMMAND);
			    }

                            /* Close lteControl file */
                            fclose(f);
                        }
                    }
                    i += (sizeof(struct inotify_event)) + event->len;
                }
            }
        }
    }
}

#ifdef ZTE_LTE_SUPPORTED
/* AT Status handler handler */
static void *atStatusThreadHandler(void *ptr)
{
    ATPort *port;
    char   imei[32] = {"N/A"};
    char   imsi[32] = {"N/A"};
    char   iccid[32] = {"N/A"};
    char   msisdn[32] = {"N/A"};
    FILE   *infoFile = NULL;

    /* Delay for the modem to settle */
    sleep(AT_STATUS_INIT_DELAY);

    /* Open port */
    while ((port = atOpen(LTE_ZTEWELINK_AT_PORT, LTE_ZTEWELINK_AT_SPEED,
                          LTE_ZTEWELINK_AT_FLOW)) == NULL) {
        syslog(LOG_ERR, "LTE AT status - cannot open AT control port.\n");
        sleep(5);
    }

    /* Make sure dongle is in correct state for ether_cdc support */
    atSend(port, LTE_ZTEWELINK_AT_ZSWITCH_CHECK, DEFAULT_AT_TIMEOUT);
    if (port->status == OK) {
        if (strstr(port->data, ": L") == NULL) {
	    syslog(LOG_ERR, "LTE: Need to set module mode to Linux\n");
	    atSend(port, LTE_ZTEWELINK_AT_ZSWITCH_SET, DEFAULT_AT_TIMEOUT);
	    if (port->status != OK) {
	        syslog(LOG_ERR, "LTE: Cannot set LTE module mode!\n");
		atClose(port);
		return NULL;
	    }

	    // Reset LTE module and wait for it to restart
	    atClose(port);
	    lteModuleReset();
	    sleep(MODULE_REBOOT_DELAY);
	    port = atOpen(LTE_ZTEWELINK_AT_PORT, LTE_ZTEWELINK_AT_SPEED,
			  LTE_ZTEWELINK_AT_FLOW);
        }
    }

    /* Get fixed status - SIM info, etc */
    atSend(port, LTE_ZTEWELINK_AT_IMEI, DEFAULT_AT_TIMEOUT);
    if (port->status == OK) {
        // Only want the digits
        if (port->data[0] != '\0') {
            strncpy(imei, port->data, 15);
        }
    }
    atSend(port, LTE_ZTEWELINK_AT_IMSI, DEFAULT_AT_TIMEOUT);
    if (port->status == OK) {
        // Only want the digits
        if (port->data[0] != '\0') {
            strncpy(imsi, port->data, 15);
        }
    }
    atSend(port, LTE_ZTEWELINK_AT_ICCID, DEFAULT_AT_TIMEOUT);
    if (port->status == OK) {
        // Only want the digits
        if (port->data[0] != '\0') {
            strncpy(iccid, &port->data[12], 20);
        }
    }
    atSend(port, LTE_ZTEWELINK_AT_MSISDN, DEFAULT_AT_TIMEOUT);
    if (port->status == OK) {
        // Only want the digits
        // Data format: +CNUM: ,"13052154423",129,7,4
        if (port->data[0] != '\0') {
            strncpy(msisdn, & port->data[9], 11);
            msisdn[11] = '\0';
        }
    }

    /* Log SIM info to file */
    infoFile = fopen(LTE_INFO_FILE, "w");
    if (infoFile) {
        fprintf(infoFile, "imei: %s\n", imei);
        fprintf(infoFile, "imsi: %s\n", imsi);
        fprintf(infoFile, "iccid: %s\n", iccid);
        fprintf(infoFile, "msisdn: %s\n", msisdn);
        fclose(infoFile);
    }

    /* Close port - we don't want to keep it open all the time */
    atClose(port);

    /* Check for periodic status like signal strength, etc */
    while (1) {
        int    signalStrength = 0;
        int    network = 0;
        int    simStatus = 0;
        char   connStatus[8] = {"N/A"};

        /* Only need to check periodically */
        sleep(AT_STATUS_CHECK_DELAY);

        port = atOpen(LTE_ZTEWELINK_AT_PORT, LTE_ZTEWELINK_AT_SPEED,
                      LTE_ZTEWELINK_AT_FLOW);
        if (port == NULL) {
            continue;
        }

        atSend(port, LTE_ZTEWELINK_AT_SIGNAL, DEFAULT_AT_TIMEOUT);
        if (port->status == OK) {
            // Data format: +CSQ: 16,99
            if (port->data[0] != '\0') {
                int a, b;
                if (sscanf(&port->data[6], "%d,%d", &a, &b) == 2) {
                    signalStrength = a;
                }
            }
        }

        atSend(port, LTE_ZTEWELINK_AT_INFO, DEFAULT_AT_TIMEOUT);
        if (port->status == OK) {
            // Data format: ^SYSINFO: 2,3,0,9,1
            if (port->data[0] != '\0') {
                int a, b, c, d, e;
                if (sscanf(&port->data[10], "%d,%d,%d,%d,%d",
                           &a, &b, &c, &d, &e) == 5) {
                    snprintf(connStatus, sizeof(connStatus), "%d:%d", a, b);
                    network = d;
                    simStatus = e;
                }
            }
        }

        FILE *statusFile = fopen(LTE_NET_STATUS_FILE, "w");
        if (statusFile) {
            fprintf(infoFile, "sim: %d\n", simStatus);
            fprintf(infoFile, "signal: %d\n", signalStrength);
            fprintf(infoFile, "status: %s\n", connStatus);
            fprintf(infoFile, "type: %d\n", network);
            fclose(statusFile);
        }

        /* Close port, can't keep it open in case needed by others */
        atClose(port);
    }
}
#endif

/* Configure for primary connection */
static int configPrimary()
{
    FILE *f;
    int  res = -1;

    f = fopen(PRI_GW_FILE, "r");
    if (f) {
        char cmd[128];
        char gw[128] = { 0 };
        char *intf;

        fscanf(f, "%s", gw);
        fclose(f);

        // If Wifi is up, use over Ethernet
        if (IRIS_isIntfIPUp(PRIMARY_WLAN_IF)) {
            intf = PRIMARY_WLAN_IF;
        } else {
            intf = PRIMARY_ETH_IF;
        }

        snprintf(cmd, sizeof(cmd),
                 "/sbin/ip route | grep -v 'metric 2' | grep 'default via %s dev %s'", 
                 gw, intf);
	f = popen(cmd, "r");
	if (f) {
	    char line[256];
	    if (fgets(line, sizeof(line), f) == NULL) {
	        snprintf(cmd, sizeof(cmd),
			 "/sbin/ip route del default > /dev/null 2>&1");
		system(cmd);
		snprintf(cmd, sizeof(cmd),
		  "/sbin/ip route add default via %s metric 0 > /dev/null 2>&1",
			 gw);
		if (!system(cmd)) {
		    snprintf(cmd, sizeof(cmd), "cp %s %s", PRI_DNS_FILE,
			     ORIG_DNS_FILE);
		    res = system(cmd);
		} else {
		    syslog(LOG_ERR, "Error configuring primary route!");
		    res = 1;
		}
	    }
	    pclose(f);
        }
    }
    return res;
}

/* Configure for backup connection */
static int configBackup(char *intf)
{
    FILE *f;
    int  res = -1;

    f = fopen(BKUP_GW_FILE, "r");
    if (f) {
        char cmd[128];
        char gw[128] = { 0 };

        fscanf(f, "%s", gw);
        fclose(f);

        snprintf(cmd, sizeof(cmd),
                 "/sbin/ip route | grep 'default via %s dev %s'", gw, intf);
	f = popen(cmd, "r");
	if (f) {
	    char line[256];
	    if (fgets(line, sizeof(line), f) == NULL) {
	        snprintf(cmd, sizeof(cmd),
			 "/sbin/ip route del default > /dev/null 2>&1");
		system(cmd);
		snprintf(cmd, sizeof(cmd),
		  "/sbin/ip route add default via %s metric 0 > /dev/null 2>&1",
			 gw);
		if (!system(cmd)) {
		    snprintf(cmd, sizeof(cmd), "cp %s %s", BKUP_DNS_FILE,
			     ORIG_DNS_FILE);
		    res = system(cmd);
		} else {
		    syslog(LOG_ERR, "Error configuring backup route!\n");
		}
	    }
	    pclose(f);
        }
    }
    return res;
}

/* Configure higher metric route to primary connection */
static void configRouteViaPriGateway()
{
    FILE *f;

    f = fopen(PRI_GW_FILE, "r");
    if (f) {
        char cmd[128];
        char gw[128] = { 0 };

        fscanf(f, "%s", gw);
        fclose(f);

        snprintf(cmd, sizeof(cmd), "/sbin/ip route | grep 'metric 2'");
	f = popen(cmd, "r");
	if (f) {
	    char line[256];
	    if (fgets(line, sizeof(line), f) == NULL) {
	        // Need to add temporary route to make sure we can get out
	        snprintf(cmd, sizeof(cmd),
		      "/sbin/route add default gw %s metric 2 > /dev/null 2>&1",
			 gw);
		system(cmd);
	    }
	    pclose(f);
        }
    }
}

/* Monitor LTE control file used by agent to start/stop LTE */
int main(int argc, char** argv)
{
    pthread_t  lte_thread, at_thread;
    int        primary_up = 1;
    FILE       *f;
    char       dongleID[128];
    char       backup_if[32];
    char       cmd[128];

    /* Turn ourselves into a daemon */
    daemon(1, 1);

    /* Catch signals so we can bring down connection */
    signal(SIGHUP, iris4g_sig_handler);
    signal(SIGTERM, iris4g_sig_handler);

    /* Setup syslog */
    openlog(SYSLOG_IDENT, (LOG_CONS | LOG_PID | LOG_PERROR), LOG_DAEMON);

    /* Create LTE control handler */
    pthread_create(&lte_thread, NULL, &lteControlThreadHandler, NULL);

    /* Make sure LTE modem is present - wait for this file to exist! */
    while (!done) {
        f = fopen(LTE_DONGLE_FILE, "r");
        dongleID[0] = '\0';
        if (f) {
            fscanf(f, "%s", dongleID);
            fclose(f);
        }
        if (dongleID[0] != '\0') {
            done = 1;
        } else {
            sleep(GATEWAY_CHECK_DELAY);
        }
    }
    done = 0;

    // What type of dongle is it?
    if (strncmp(dongleID, LTE_HUAWEI_MODEM_ID,
		strlen(LTE_HUAWEI_MODEM_ID)) == 0) {
        snprintf(backup_if, sizeof(backup_if), "%s", BACKUP_ETH_IF);
#ifdef QUECTEL_LTE_SUPPORTED
    } else if (strncmp(dongleID, LTE_QUECTEL_MODEM_ID,
		       strlen(LTE_QUECTEL_MODEM_ID)) == 0) {
        snprintf(backup_if, sizeof(backup_if), "%s", BACKUP_WWAN_IF);
#endif
#ifdef ZTE_LTE_SUPPORTED
    } else if (strncmp(dongleID, LTE_ZTEWELINK_MODEM_ID,
		       strlen(LTE_ZTEWELINK_MODEM_ID)) == 0) {
        snprintf(backup_if, sizeof(backup_if), "%s", BACKUP_USB_IF);

        /* Create AT status thread */
        pthread_create(&at_thread, NULL, &atStatusThreadHandler, NULL);
#endif
    } else {
        snprintf(backup_if, sizeof(backup_if), "Unknown");
    }

    /* Record interface for agent access */
    snprintf(cmd, sizeof(cmd), "echo %s > %s", backup_if, LTE_INTF_FILE);
    system(cmd);

    /* Check on primary gateway */
    while (!done) {
        char state[16];

        /* Do this every so often */
        sleep(GATEWAY_CHECK_DELAY);

        /* If we are testing the wifi connection, don't get in the way */
        if (access("/tmp/testWifi.cfg", F_OK) != -1) {
            continue;
        }

        /* If backup connection is in use, check if primary gateway is back */
        snprintf(state, sizeof(state), "down");
        snprintf(cmd, sizeof(cmd), BACKUP_CHECK_IF_STATUS, backup_if);
        f = popen(cmd, "r");
        if (f) {
            char line[256];
            if (fgets(line, sizeof(line), f) != NULL) {
                snprintf(state, sizeof(state), "up");
            }
            pclose(f);
        }

        // Update status looked at by the agent
        f = fopen(LTE_STATUS_FILE, "r");
        if (f) {
            char oldstate[16];
            fscanf(f, "%s", oldstate);
            fclose(f);
            if (strncmp(state, oldstate, sizeof(oldstate))) {
                snprintf(cmd, sizeof(cmd), "echo %s > %s",
                         state, LTE_STATUS_FILE);
                system(cmd);
            }
        }

        // Is backup connection up?
        if (strncmp(state, "up", 2) == 0) {

            // Add metric 2 route out primary, if not there
            configRouteViaPriGateway();

            // Check if primary is back up
            if (!system(CHECK_PRIMARY_CMD)) {
                if (!primary_up) {
                    // It's back - notify agent
                    snprintf(cmd, sizeof(cmd), "echo available > %s",
                             PRI_GW_AVAIL_FILE);
                    system(cmd);

                    // Restore primary route and DNS
                    if (!configPrimary()) {
                        primary_up = 1;
                    }
                }
            } else {
                // Setup backup gw and DNS
                if (!configBackup(backup_if)) {
                    primary_up = 0;
                    unlink(PRI_GW_AVAIL_FILE);
                }
            }
        } else {
            // Use primary route and DNS
            if (!primary_up && !configPrimary()) {
                primary_up = 1;
            }
        }
    }
    return 0;
}


