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
#include <sys/types.h>
#include <sys/stat.h>
#include <irislib.h>
#include "build_image.h"


#define TMP_FILE       "/tmp/upload.bin"
// Back to using curl as we need it for backup connection support
#define USE_CURL       1

static void usage(char *name)
{
    fprintf(stderr, "\nusage: %s [options] URL\n"
            "  options:\n"
            "    -h              Print this message\n"
            "    -c user:passwd  User username/password for login\n"
            "    -f              Force install even if version is the same\n"
            "    -k              Kill agent before installing radio firmware\n"
            "                    Update to firmware found at URL\n"
            "\n", name);
}

/* Update device firmware via file@URL */
int main(int argc, char** argv)
{
    int  c, force = 0, credentials = 0, killAgent = 0;
    char *urlarg = NULL, *credarg = NULL;
    char cmd[256];
    char ledMode[128] = { 0 };
    int  ledDelay = 3;
    int  res = 0;

    /* Parse options... */
    opterr = 0;
    while ((c = getopt(argc, argv, "hfkc:")) != -1)
    switch (c) {
    case 'h':
        usage(argv[0]);
        return 0;
    case 'f':
        force = 1;
        break;;
    case 'k':
        killAgent = 1;
        break;;
    case 'c':
        credentials = 1;
        credarg = optarg;
        break;;
    case '?':
        fprintf(stderr, "Unknown option `\\x%x'.\n", optopt);
        usage(argv[0]);
        exit(INSTALL_ARG_ERR);
    default:
        fprintf(stderr, "Error parsing options - exiting!\n");
        usage(argv[0]);
        exit(INSTALL_ARG_ERR);
    }

    /* Must have the URL as argument */
    urlarg = argv[optind];
    if (urlarg == NULL) {
        fprintf(stderr, "Incorrect number of arguments - exiting!\n");
        usage(argv[0]);
        exit(INSTALL_ARG_ERR);
    }

    /* Make sure old firmware files has been deleted */
    remove(FW_INSTALL_FILE);
    remove(TMP_FILE);

    /* Is this a local file? */
    if (strncmp(urlarg, "file://", 7) == 0) {
        char *filename;

        /* Handle if user forgets extra / or adds localhost */
        if (strncmp(urlarg, "file://localhost", 16) == 0) {
            filename = &urlarg[16];
        } else if (urlarg[7] != '/') {
            filename = &urlarg[6];
        } else {
            filename = &urlarg[7];
        }
        snprintf(cmd, sizeof(cmd), "ln -s %s %s", filename, TMP_FILE);
    } else {
#ifdef USE_CURL
        /* Create curl command to get file */
        if (credentials) {
            snprintf(cmd, sizeof(cmd), "curl -s -k -u %s %s -o %s",
                     credarg, urlarg, TMP_FILE);
        } else {
            snprintf(cmd, sizeof(cmd), "curl -s %s -o %s", urlarg, TMP_FILE);
        }
#else
        /* Default to busybox wget support
           - no HTTPS, just HTTP, TFTP and FTP though */
        if (strncmp(urlarg, "tftp://", 7) == 0) {
            char tftpURL[128];
            char *remote, *server;

            strncpy(tftpURL, &urlarg[7], sizeof(tftpURL));
            server = tftpURL;
            remote = strchr(server, '/');
            if (remote) {
                *remote++ = '\0';
                snprintf(cmd, sizeof(cmd), "tftp -l %s -r %s -g %s", TMP_FILE,
                         remote, server);
            } else {
                fprintf(stderr, "Bad URL format - exiting!\n");
                exit(INSTALL_ARG_ERR);
            }
        } else if (strncmp(urlarg, "http://", 7) == 0) {
            if (credentials) {
                snprintf(cmd, sizeof(cmd), "wget -q -O %s http://%s@%s",
                         TMP_FILE, credarg, &urlarg[7]);
            } else {
                snprintf(cmd, sizeof(cmd), "wget -q -O %s %s", TMP_FILE,
                         urlarg);
            }
        } else if (strncmp(urlarg, "ftp://", 6) == 0) {
            if (credentials) {
                snprintf(cmd, sizeof(cmd), "wget -q -O %s ftp://%s@%s",
                         TMP_FILE, credarg, &urlarg[7]);
            } else {
                snprintf(cmd, sizeof(cmd), "wget -q -O %s %s", TMP_FILE,
                         urlarg);
            }
        } else {
            fprintf(stderr, "Unsupported URL format - exiting!\n");
            exit(INSTALL_ARG_ERR);
        }
#endif
    }

    /* Get file */
    fprintf(stdout, "Downloading file..");
    fflush(stdout);
    if (system(cmd)) {
        fprintf(stderr, "Error downloading firmware file!\n");
        exit(INSTALL_DOWNLOAD_ERR);
    }
    fprintf(stdout, ".done\n");

    /* Get current LED mode so we can restore later */
    IRIS_getLedMode(ledMode, sizeof(ledMode));

    /* Create command to validate file */
    if (force) {
        snprintf(cmd, sizeof(cmd), "validate_image -s %s", TMP_FILE);
    } else {
        snprintf(cmd, sizeof(cmd), "validate_image %s", TMP_FILE);
    }

    /* Validate file */
    if ((res = system(cmd))) {
        fprintf(stderr, "Error validating firmware file!\n");
    } else {

        /* Install file if file not the same */
        if (access(FW_INSTALL_FILE, F_OK) != -1) {
            /* Should we kill the agent first if installing radio firmware? */
            if (killAgent) {
                snprintf(cmd, sizeof(cmd), "fwinstall -k %s", FW_INSTALL_FILE);
            } else {
                snprintf(cmd, sizeof(cmd), "fwinstall %s", FW_INSTALL_FILE);
            }
            if ((res = system(cmd))) {
                fprintf(stderr, "Error installing firmware!\n");
                remove(TMP_FILE);
                /* Hard error? - leave LEDs on for 10 seconds until reboot */
                if (res >= INSTALL_HARD_ERRORS) {
                    ledDelay = 10;
                }
            } else {
                fprintf(stdout, "\nFirmware update was successful "
                        "- please reboot to run latest firmware.\n");
            }
        }
    }

    /* Restore LEDs */
    if (res) {
        /* Wait up to 10 seconds for flashing to stop then restore - we
           may have rebooted on a hard error! */
        sleep(ledDelay);
    }
    IRIS_setLedMode(ledMode);

    /* Clean up */
    remove(FW_INSTALL_FILE);
    remove(TMP_FILE);
    if (res) {
        exit(res);
    }
    return 0;
}
