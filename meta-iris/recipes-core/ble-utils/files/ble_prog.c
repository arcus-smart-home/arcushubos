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
#include <fcntl.h>
#include <termios.h>
#include <irislib.h>


/* Must be implemented by vendor-specific file */
extern int BLE_vendorProgram(int fd, char *filename, int slot);


/* Reset routines are independent of hardware BLE chip */
void BLE_assertReset(void)
{
    char cmd[128];

    /* Reset BLE module via GPIO */
    snprintf(cmd, sizeof(cmd), "echo 0 > %s", BLE_RESET_GPIO_VALUE_FILE);
    system(cmd);
    // printf("Drop BLE reset line\n");
    usleep(100000);
}

void BLE_deassertReset(void)
{
    char cmd[128];

    /* Clear BLE reset line */
    snprintf(cmd, sizeof(cmd), "echo 1 > %s", BLE_RESET_GPIO_VALUE_FILE);
    system(cmd);
    // printf("Clear BLE reset line\n");
    usleep(100000);
}

static void usage(char *name)
{
    fprintf(stderr, "\nusage: %s [options] filename\n"
            "  options:\n"
            "    -h           Print this message\n"
            "    -s slot      Slot to program (for Nordic/mcuboot BLE)\n"
            "    filename     Program flash with binary data from file\n"
            "\n", name);
}

// Programming support for BLE, assuming part has serial bootloader already
int main(int argc, char** argv)
{
    int             fd;
    char            *filearg = NULL;
    int             c;
    int             slot = 0;

    /* Parse options */
    opterr = 0;
    while ((c = getopt(argc, argv, "hs:")) != -1)
    switch (c) {
    case 'h':
        usage(argv[0]);
        return 0;
    case 's':
        slot = atoi(optarg);
        break;
    case '?':
        fprintf(stderr, "Unknown option `\\x%x'.\n", optopt);
        usage(argv[0]);
        return 1;
    default:
        fprintf(stderr, "Error parsing options - exiting!\n");
        usage(argv[0]);
        return 1;
    }

    // Must have the filename as argument
    filearg = argv[optind];
    if (filearg == NULL) {
        fprintf(stderr, "Incorrect number of arguments - exiting!\n");
        usage(argv[0]);
        exit(1);
    }

    // Open serial port
    fd = open(BLE_UART, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "Unable to open BLE UART - exiting!\n");
        exit(1);
    }

    // Set port settings
    IRIS_initSerialPort(fd, B115200, FLOW_CONTROL_RTSCTS);

    // Run vendor specific programming code
    if (BLE_vendorProgram(fd, filearg, slot) == 0) {
        printf("\n BLE Programming is complete.\n");
        close(fd);
        return 0;
    } else {
        close(fd);
        exit(1);
    }
}
