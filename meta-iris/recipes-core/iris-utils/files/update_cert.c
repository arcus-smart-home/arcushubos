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
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <irislib.h>


static void usage(char *name)
{
    fprintf(stderr, "\nusage: %s [options] filename\n"
            "  options:\n"
            "    -h              Print this message\n"
            "                    Install certificate from file\n"
            "\n", name);
}


/* Update manufacturing certificate file */
int main(int argc, char** argv)
{
    int  c, res = 0;
    char *filename;
    char cmd[256];
    FILE *f;
    char buf[512];
    char macAddr[24];

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

    /* Must have the filename as argument */
    filename = argv[optind];
    if (filename == NULL) {
        fprintf(stderr, "Incorrect number of arguments - exiting!\n");
        usage(argv[0]);
        exit(1);
    }

    /* Mount mfg partition */
    if (mount(MFG_MOUNT_DEV, MFG_MOUNT_DIR, MFG_MOUNT_FSTYPE, MS_NOEXEC,
              NULL)) {
        fprintf(stderr, "Unable to mount mfg partition!\n");
        exit(1);
    }

    /* Create command to validate certificate */
    snprintf(cmd, sizeof(cmd), "openssl verify -CAfile %s %s",
             HUB_CA_CERT, filename);

    /* Validate certificate */
    if ((res = system(cmd))) {
    validate_error:
        fprintf(stderr, "Error validating certificate file!\n");
        goto exit;
    }

    /* Make sure serial number matches MAC address */
    snprintf(cmd, sizeof(cmd), "openssl x509 -subject -in %s -noout", filename);
    f = popen(cmd, "r");
    if ((f == NULL) || (fgets(buf, sizeof(buf), f) == NULL)) {
        res = 1;
        if (f) pclose(f);
        goto validate_error;
    }
    pclose(f);

    /* Get device MAC address */
    f = fopen(MFG_MAC1_FILE, "r");
    if ((f == NULL) || (fgets(macAddr, sizeof(macAddr), f) == NULL)) {
        res = 1;
        if (f) fclose(f);
        goto validate_error;
    }
    fclose(f);
    /* Remove trailing \n */
    if (macAddr[strlen(macAddr)-1] == '\n') {
        macAddr[strlen(macAddr)-1] = '\0';
    }

    /* Make sure MAC address is in cert subject */
    if (strstr(buf, macAddr) == NULL) {
        res = 1;
        goto validate_error;
    }

    /* Move file to mfg partition */
    snprintf(buf, sizeof(buf), "%s/%s.crt", MFG_CERTS_DIR, macAddr);
    snprintf(cmd, sizeof(cmd), "%s.last", buf);
    rename(buf, cmd);
    if (IRIS_copyFile(filename, buf)) {
        fprintf(stderr, "Unable to rename certificate file!\n");
	rename(cmd, buf);
	perror(strerror(errno));
        res = 1;
    }
    unlink(cmd);

    /* Make sure root owns the file */
    chmod(buf, S_IRUSR | S_IRGRP | S_IROTH);
    chown(buf, 0, 0);

 exit:
    /* Unmount mfg partition - don't want others to touch this! */
    if (umount(MFG_MOUNT_DIR)) {
        fprintf(stderr, "Unable to unmount mfg partition!\n");
        res = 1;
    }

    /* Clean up */
    if (res) {
        exit(1);
    } else {
        fprintf(stdout, "\nCertificate update was successful\n");
    }
    return 0;
}
