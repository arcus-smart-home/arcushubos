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
#include <irislib.h>
#include "backup.h"

#define CHECK_HUB_URL   "http://104.46.102.172/check-hub"
#define CHECK_TIMEOUT   5
#define CURL_CMD        "/usr/bin/curl -m %d --interface %s %s"

static void usage(char *name)
{
    fprintf(stderr, "\nusage: %s [options] URL\n"
            "  options:\n"
            "    -h              Print this message\n"
            "                    Check primary connection via http get of URL\n"
            "\n", name);
}

/* Check primary network connection */
int main(int argc, char** argv)
{
    int  c;
    char *host;
    int pipes[2];
    char *url, cmd[256], *intf;
    FILE *f;

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

    // If no value, assume use default
    url = argv[optind];
    if (url == NULL) {
        url = CHECK_HUB_URL;
    }

    // Redirect output so it doesn't show on console
    pipe(pipes);
    dup2(pipes[0], fileno(stderr));
    dup2(pipes[1], fileno(stdout));

    // If Wifi is up, use over Ethernet
    if (IRIS_isIntfIPUp(PRIMARY_WLAN_IF)) {
        intf = PRIMARY_WLAN_IF;
    } else {
        intf = PRIMARY_ETH_IF;
    }

    // Check hub URL
    snprintf(cmd, sizeof(cmd), CURL_CMD, CHECK_TIMEOUT, intf, url);
    f = popen(cmd, "r");
    if (f) {
        char line[256];
        if ((fgets(line, sizeof(line), f) != NULL) &&
	    (strncmp(line, "up", 2) == 0)) {
	    pclose(f);
	    return 0;
	}
        pclose(f);
    }
    return -1;
}
