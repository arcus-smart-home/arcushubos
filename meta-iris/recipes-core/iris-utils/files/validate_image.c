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
#include <sys/mman.h>
#include <fcntl.h>
#include <irislib.h>
#include "build_image.h"

#define HEADER_SIGNED_FILE "/tmp/header.signed"
#define HEADER_FILE        "/tmp/header.verified"
#define FW_ENCRYPT_FILE    "/tmp/firmware.encrypt"

static fw_header_t header;

static void usage(char *name)
{
    fprintf(stderr, "\nusage: %s [options] firmware_file\n"
            "  options:\n"
            "    -h          Print this message\n"
            "    -m          Mfgtool support (just extract file)\n"
            "    -s          Skip version comparision check\n"
            "                Validate firmware file and create install file\n"
            "\n", name);
}

/* Validate firmware file and create image to install */
int main(int argc, char** argv)
{
    int  c, skip = 0, mfgMode = 0, res = 0;
    char *filearg = NULL;
    FILE *tempf;
    int  fd;
    char *map = NULL;
    struct stat fs;
    char cmd[256];
    char buf[256];
    char fw_cksum[64];

    /* Parse options... */
    opterr = 0;
    while ((c = getopt(argc, argv, "hms")) != -1)
    switch (c) {
    case 'h':
        usage(argv[0]);
        return 0;
    case 'm':
        mfgMode = 1;
        break;;
    case 's':
        skip = 1;
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

    /* Must have the file archive as argument */
    filearg = argv[optind];
    if (filearg == NULL) {
        fprintf(stderr, "Incorrect number of arguments - exiting!\n");
        usage(argv[0]);
        exit(INSTALL_ARG_ERR);
    }

    /* Open file */
    fd = open(filearg, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "Unable to open firmware file %s\n", filearg);
        exit(INSTALL_FILE_OPEN_ERR);
    }

    /* Get file information */
    if (fstat(fd, &fs)) {
        fprintf(stderr, "Unable to get size of firmware file %s\n", filearg);
        res = INSTALL_FILE_OPEN_ERR;
        goto error_exit;
    }

    /* Set LED to decrypt setting */
    snprintf(cmd, sizeof(cmd), "echo upgrade-decrypt > %s", LED_MODE_FILE);
    system(cmd);

    /* Map file */
    map = mmap(NULL, fs.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (map == MAP_FAILED) {
        fprintf(stderr, "Error creating memory map\n");
        close(fd);
        res = INSTALL_MEMORY_ERR;
        goto error_exit;
    }

    /* Signed header is first block of file */
    tempf = fopen(HEADER_SIGNED_FILE, "w");
    if (tempf == NULL) {
        fprintf(stderr, "Unable to open header file!\n");
        res = INSTALL_DECRYPT_ERR;
        goto error_exit;
    }
    if (fwrite(map, 1, SIGNED_HDR_LEN, tempf) != SIGNED_HDR_LEN) {
        fprintf(stderr, "Unable to extract signed header!\n");
        res = INSTALL_DECRYPT_ERR;
        goto error_exit;
    }
    fclose(tempf);
    snprintf(cmd, sizeof(cmd),
             "openssl rsautl -verify -in %s -pubin -inkey %s -out %s",
             HEADER_SIGNED_FILE, BUILD_PUBLIC_KEY, HEADER_FILE);
    if (system(cmd)) {
        fprintf(stderr, "Unable to verify signed header!\n");
        res = INSTALL_HDR_SIG_ERR;
        goto error_exit;
    }

    /* Read header */
    tempf = fopen(HEADER_FILE, "r");
    if (tempf == NULL) {
        fprintf(stderr, "Unable to open header file!\n");
        res = INSTALL_DECRYPT_ERR;
        goto error_exit;
    }
    if (fread((void *)&header, 1, sizeof(header), tempf) != sizeof(header)) {
        fprintf(stderr, "Unable to read header information!\n");
        fclose(tempf);
        res = INSTALL_DECRYPT_ERR;
        goto error_exit;
    }
    fclose(tempf);

    // For mfgtool purpose, skip hub related checks
    if (mfgMode) {
        goto decrypt;
    }

    /* Check version */
    fprintf(stderr, "Firmware version is: %s\n", header.fw_version);
    if (!skip) {
        tempf = fopen(VERSION_FILE, "r");
        if (tempf == NULL) {
            fprintf(stderr, "Unable to open version file!\n");
            res = INSTALL_DECRYPT_ERR;
            goto error_exit;
        }
        if (fscanf(tempf, "%s", buf) != 1) {
            fprintf(stderr, "Unable to read version file!\n");
            pclose(tempf);
            res = INSTALL_DECRYPT_ERR;
            goto error_exit;
        }
        fclose(tempf);

        /* No need to install if same version */
        if (strncmp(buf, header.fw_version, strlen(header.fw_version)) == 0) {
            fprintf(stderr,
                    "Firmware version is the same as current firmware "
                    "- skipping install\n");
            remove(FW_INSTALL_FILE);

            /* This is not an error! */
            return 0;
        }
    }

    /* Check model */
    fprintf(stderr, "Firmware model is: %s\n", header.fw_model);
    tempf = fopen(MODEL_FILE, "r");
    if (tempf == NULL) {
        fprintf(stderr, "Unable to open model file!\n");
        res = INSTALL_DECRYPT_ERR;
        goto error_exit;
    }
    if (fscanf(tempf, "%s", buf) != 1) {
        fprintf(stderr, "Unable to read model file!\n");
        pclose(tempf);
        res = INSTALL_DECRYPT_ERR;
        goto error_exit;
    }
    fclose(tempf);

    /* Just compare the first 4 characters - allows us to have the
       same firmware for IH300, IH304, etc.  */
    if (strncmp(buf, header.fw_model, 4) != 0) {
        fprintf(stderr, "Firmware model does not match - skipping install\n");
        res = INSTALL_MODEL_ERR;
        goto error_exit;
    }

    /* Check customer */
    fprintf(stderr, "Firmware customer is: %s\n", header.fw_customer);
    tempf = fopen(CUSTOMER_FILE, "r");
    if (tempf == NULL) {
        fprintf(stderr, "Unable to open customer file!\n");
        res = INSTALL_DECRYPT_ERR;
        goto error_exit;
    }
    if (fscanf(tempf, "%s", buf) != 1) {
        fprintf(stderr, "Unable to read customer file!\n");
        pclose(tempf);
        res = INSTALL_DECRYPT_ERR;
        goto error_exit;
    }
    fclose(tempf);

    /* Customer must match, but allow "ALL" match as well */
    if ((strncmp("ALL", header.fw_customer, strlen(header.fw_customer)) != 0) &&
        (strncmp(buf, header.fw_customer, strlen(header.fw_customer)) != 0)) {
        fprintf(stderr,
                "Firmware customer does not match - skipping install\n");
        res = INSTALL_CUSTOMER_ERR;
        goto error_exit;
    }

    /* Decrypt file */
 decrypt:
    fprintf(stdout, "Decrypting firmware file...");
    tempf = fopen(FW_ENCRYPT_FILE, "w");
    if (tempf == NULL) {
        fprintf(stderr, "Unable to open encrypted firmware file!\n");
        res = INSTALL_DECRYPT_ERR;
        goto error_exit;
    }
    if (fwrite((map + SIGNED_HDR_LEN), 1, (fs.st_size - SIGNED_HDR_LEN),
               tempf) != (fs.st_size - SIGNED_HDR_LEN)) {
        fprintf(stderr, "Unable to extract encrypted firmware file!\n");
        res = INSTALL_DECRYPT_ERR;
        goto error_exit;
    }
    fclose(tempf);
    snprintf(cmd, sizeof(cmd),
             "openssl aes-128-cbc -in %s -out %s -e -K %s -iv %s -d",
             FW_ENCRYPT_FILE, FW_INSTALL_FILE, header.fw_key, header.fw_iv);
    if (system(cmd)) {
        fprintf(stderr, "\nUnable to decrypt firmware file!\n");
        res = INSTALL_FW_DECRYPT_ERR;
        goto error_exit;
    }
    fprintf(stdout, "Done.\n");

    /* Run sha256sum on file to get checksum */
    snprintf(cmd, sizeof(cmd), "sha256sum -b %s", FW_INSTALL_FILE);
    tempf = popen(cmd, "r");
    if (tempf == NULL) {
        fprintf(stderr, "Unable to calculate file checksum!\n");
        res = INSTALL_DECRYPT_ERR;
        goto error_exit;
    }
    if (fscanf(tempf, "%s", fw_cksum) != 1) {
        fprintf(stderr, "Unable to read file checksum!\n");
        pclose(tempf);
        res = INSTALL_DECRYPT_ERR;
        goto error_exit;
    }
    pclose(tempf);

    /* Check checksum! */
    if (memcmp(fw_cksum, header.fw_cksum, sizeof(fw_cksum))) {
        fprintf(stderr, "Firmware file checksum is incorrect!\n");
        res = INSTALL_CKSUM_ERR;
        goto error_exit;
    }

    /* Clean up */
    close(fd);
    if (map) {
        munmap(map, fs.st_size);
    }
    remove(FW_ENCRYPT_FILE);
    remove(HEADER_SIGNED_FILE);
    remove(HEADER_FILE);

    fprintf(stdout, "\nFirmware image validation passed!\n");
    return 0;

 error_exit:
    /* Set LED to decrypt-err setting */
    snprintf(cmd, sizeof(cmd), "echo upgrade-decrypt-err > %s", LED_MODE_FILE);
    system(cmd);

    /* Make sure firmware file is not left around! */
    remove(FW_INSTALL_FILE);

    /* Clean up file mapping */
    close(fd);
    if (map) {
        munmap(map, fs.st_size);
    }

    /* Remove temporary files as well */
    remove(FW_ENCRYPT_FILE);
    remove(HEADER_SIGNED_FILE);
    remove(HEADER_FILE);
    exit(res);
}
