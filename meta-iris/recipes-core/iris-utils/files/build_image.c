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
#include <arpa/inet.h>
#include <irislib.h>
#include "build_image.h"


#define BUILD_PRIVATE_KEY  "iris-fwsign-nopass.key"
#define HEADER_ORIG_FILE   "header.aes"
#define HEADER_SIGNED_FILE "header.signed"
#define IMAGE_ENCRYPT_FILE "image.encrypted"

static fw_header_t header;

static void usage(char *name)
{
    fprintf(stderr, "\nusage: %s [options] firmware_tar\n"
            "  options:\n"
            "    -h          Print this message\n"
            "    -c customer Set device customer (default is %s)\n"
            "    -m model    Set model name (default is %s)\n"
            "    -p prefix   Use string as output file prefix\n"
            "                Build signed image from firmware tgz file\n"
            "\n", name, DEFAULT_MODEL, DEFAULT_CUSTOMER);
}

char *validate_model(char *model)
{
    /* Only one valid model for now - the default */
    if ((model == NULL) ||
        (strncmp(model, DEFAULT_MODEL, sizeof(DEFAULT_MODEL)-1) == 0)) {
        return DEFAULT_MODEL;
    }
    return NULL;
}

char *validate_customer(char *customer)
{
    /* Only one valid customer for now - the default, and "ALL" for any */
    if ((customer != NULL) && (strncmp(customer, CUSTOMER_ALL,
				       sizeof(CUSTOMER_ALL)-1) == 0)) {
        return customer;
    } else if ((customer == NULL) ||
        (strncmp(customer, DEFAULT_CUSTOMER, sizeof(DEFAULT_CUSTOMER)) == 0)) {
        return DEFAULT_CUSTOMER;
    }
    return NULL;
}

/* Build signed image from compressed firmware tarfile */
int main(int argc, char** argv)
{
    int  c;
    char *modelarg = NULL, *customerarg = NULL, *filearg = NULL,
      *prefixarg = NULL;
    FILE *tempf;
    struct stat fstat;
    char cmd[256];

    /* Parse options... */
    opterr = 0;
    while ((c = getopt(argc, argv, "hm:p:c:")) != -1)
    switch (c) {
    case 'h':
        usage(argv[0]);
        return 0;
    case 'm':
        modelarg = optarg;
        break;
    case 'p':
        prefixarg = optarg;
        break;
    case 'c':
        customerarg = optarg;
        break;
    case '?':
        fprintf(stderr, "Unknown option `\\x%x'.\n", optopt);
        usage(argv[0]);
        exit(1);
    default:
        fprintf(stderr, "Error parsing options - exiting!\n");
        usage(argv[0]);
        exit(1);
    }

    /* Must have the file archive as argument */
    filearg = argv[optind];
    if (filearg == NULL) {
        fprintf(stderr, "Incorrect number of arguments - exiting!\n");
        usage(argv[0]);
        exit(1);
    }

    /* Check model */
    modelarg = validate_model(modelarg);
    if (modelarg == NULL) {
        fprintf(stderr, "Invalid model!\n");
        exit(1);
    }

    /* Check customer */
    customerarg = validate_customer(customerarg);
    if (customerarg == NULL) {
        fprintf(stderr, "Invalid customer!\n");
        exit(1);
    }

    /* Setup prefix */
    if (prefixarg == NULL) {
        prefixarg = DEFAULT_PREFIX;
    }

    /* Get file information */
    if (stat(filearg, &fstat)) {
        fprintf(stderr, "Unable to get size of firmware file %s\n", filearg);
        exit(1);
    }

    /* Create header data */
    memset((void *)&header, 0, sizeof(header));
    header.header_version = htons(HEADER_VERSION_0);
    header.image_size = htonl(fstat.st_size);
    snprintf(header.fw_version, sizeof(header.fw_version),
             "v%d.%d.%d.%03d%s", VERSION_MAJOR, VERSION_MINOR, VERSION_POINT,
             VERSION_BUILD, VERSION_SUFFIX);
    snprintf(header.fw_model, sizeof(header.fw_model), "%s", modelarg);
    snprintf(header.fw_customer, sizeof(header.fw_customer), "%s",
	     customerarg);

    /* Run sha256sum on file to get checksum */
    snprintf(cmd, sizeof(cmd), "sha256sum -b %s", filearg);
    tempf = popen(cmd, "r");
    if (tempf == NULL) {
        fprintf(stderr, "Unable to calculate file checksum!\n");
        exit(1);
    }
    if (fscanf(tempf, "%s", header.fw_cksum) != 1) {
        fprintf(stderr, "Unable to read file checksum!\n");
        pclose(tempf);
        exit(1);
    }
    pclose(tempf);

    /* Generate firmware key */
    snprintf(cmd, sizeof(cmd), "openssl rand -hex %d", FW_KEY_LEN);
    tempf = popen(cmd, "r");
    if (tempf == NULL) {
        fprintf(stderr, "Unable to generate firmware key!\n");
        exit(1);
    }
    if (fscanf(tempf, "%s", header.fw_key) != 1) {
        fprintf(stderr, "Unable to create firmware key!\n");
        pclose(tempf);
        exit(1);
    }
    pclose(tempf);

    /* Generate firmware initialization vector  */
    snprintf(cmd, sizeof(cmd), "openssl rand -hex %d", FW_IV_LEN);
    tempf = popen(cmd, "r");
    if (tempf == NULL) {
        fprintf(stderr, "Unable to generate firmware initialization vector!\n");
        exit(1);
    }
    if (fscanf(tempf, "%s", header.fw_iv) != 1) {
        fprintf(stderr, "Unable to create firmware initialization vector!\n");
        pclose(tempf);
        exit(1);
    }
    pclose(tempf);

    /* Write out header data to a file */
    tempf = fopen(HEADER_ORIG_FILE, "w");
    if (tempf == NULL) {
        fprintf(stderr, "Unable to create header file!\n");
        exit(1);
    }
    if (fwrite((void *)&header, 1, sizeof(header), tempf) != sizeof(header)) {
        fprintf(stderr, "Unable to write header file!\n");
        fclose(tempf);
        exit(1);
    }
    fclose(tempf);

    /* Create signed header data */
    snprintf(cmd, sizeof(cmd),
	     "openssl rsautl -sign -inkey %s -in %s -out %s",
             BUILD_PRIVATE_KEY, HEADER_ORIG_FILE, HEADER_SIGNED_FILE);
    if (system(cmd)) {
        fprintf(stderr, "Unable to create signed header!\n");
        exit(1);
    }

    /* Create encrypted image file */
    snprintf(cmd, sizeof(cmd),
	     "openssl aes-128-cbc -in %s -out %s -e -K %s -iv %s -e",
	     filearg, IMAGE_ENCRYPT_FILE, header.fw_key, header.fw_iv);
    if (system(cmd)) {
        fprintf(stderr, "Unable to encrypt firmware file!\n");
        exit(1);
    }

    /* Cat signed header and encrypted image together */
    snprintf(cmd, sizeof(cmd), "cat %s %s > %s_%d.%d.%d.%03d%s.bin",
	     HEADER_SIGNED_FILE, IMAGE_ENCRYPT_FILE, prefixarg,
             VERSION_MAJOR, VERSION_MINOR, VERSION_POINT, VERSION_BUILD,
             VERSION_SUFFIX);
    if (system(cmd)) {
        fprintf(stderr, "Unable to create final image file!\n");
        exit(1);
    }

    /* Clean up */
    remove(HEADER_ORIG_FILE);
    remove(HEADER_SIGNED_FILE);
    remove(IMAGE_ENCRYPT_FILE);

    fprintf(stdout, "\nFirmware image creation is complete.\n");
    return 0;
}
