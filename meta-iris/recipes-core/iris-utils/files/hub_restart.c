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
#include <irisdefs.h>

static void usage(char *name)
{
    fprintf(stderr, "\nusage: %s [options] when\n"
            "  options:\n"
            "    -h              Print this message\n"
            "                    Restart hub in n seconds, or \"now\"\n"
            "\n", name);
}

/* Install firmware archive */
int main(int argc, char** argv)
{
    int  c, delay;
    char *whenarg;
    char cmd[128];

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

    // If no value, assume now
    whenarg = argv[optind];
    if ((whenarg == NULL) || (strncmp(whenarg, "now", 3) == 0)) {
        delay = 0;
        fprintf(stdout, "Restarting hub now!\n");
    } else {
        delay = atoi(whenarg);
        fprintf(stdout, "Restarting hub in %d seconds...\n", delay);
    }

    // Change LEDs to indicate shutdown in process - call utility
    //  directly as there is not enough time to write to ledMode file
    //  if a delay is not given!
    snprintf(cmd, sizeof(cmd), "%s shutdown", LED_CTRL_APP);
    system(cmd);

    // Delay first
    if (delay) {
        sleep(delay);
    }

#ifdef imxdimagic
    // Leave LED ring on for 5 seconds at shutdown, so may need to delay more
    if (delay < 5) {
        sleep(5 - delay);
    }

    // Then turn on boot LEDs so ring isn't dark!
    snprintf(cmd, sizeof(cmd), "%s boot-linux", LED_CTRL_APP);
    system(cmd);
#endif

    // Reboot
    sync(); sync();
    if (system("/sbin/reboot")) {
        fprintf(stderr, "Error attempting restart: %s\n", strerror(errno));
	exit(1);
    }
    return 0;
}

