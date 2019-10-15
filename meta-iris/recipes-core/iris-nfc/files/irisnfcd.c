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
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <syslog.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <libst95hf.h>
#include <irislib.h>


#define SYSLOG_IDENT         "irisnfcd"
#define NFC_CHECK_DELAY      1

static volatile int done = 0;
static uint8_t mode = 0;
static uint8_t bits = 8;
static uint32_t speed = 1000000;

/* Initialize NFC device */
static int nfc_init()
{
    int fd;

    fd = open(NFC_DEV, O_RDWR);
    if (fd < 0) {
        syslog(LOG_ERR, "Cannot open NFC device - exiting!");
        exit(0);
    }

    // Set SPI mode
    if (ioctl(fd, SPI_IOC_WR_MODE, &mode) == -1) {
        syslog(LOG_ERR, "Cannot set SPI mode - exiting!");
        exit(0);
    }

    // Set SPI bits per word
    if (ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits) == -1) {
        syslog(LOG_ERR, "Cannot set SPI bits per word - exiting!");
        exit(0);
    }

    // Set SPI max speed hz
    if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) == -1) {
        syslog(LOG_ERR, "Cannot set SPI bits per word - exiting!");
        exit(0);
    }
    close(fd);

    /* Initialize NFC support */
    ConfigManager_HWInit();
    return 0;
}

static int nfc_getTag(char *UID)
{
    int8_t TagType = TRACK_NOTHING;
    uint8_t buf[32];

    // Set correct mode of operation
    devicemode = PCD;

    /* Scan to find if there is a tag */
    TagType = ConfigManager_TagHunting(TRACK_ALL);

    /* Stop hunting to avoid interaction issues */
    ConfigManager_Stop();

    switch(TagType) {
    case TRACK_NFCTYPE1:
    case TRACK_NFCTYPE2:
    case TRACK_NFCTYPE3:
    case TRACK_NFCTYPE4A:
    case TRACK_NFCTYPE4B:
        // FIXLATER - do we care about these other tag types?
        strcpy(UID, "N/A");
        break;
    case TRACK_NFCTYPE5:
        // Get UID
        UID[0] = '\0';
        ISO15693_GetUID(buf);
        // UID is in reverse order
        for (int i = 7; i >= 0 ; i--) {
            char  hexDigit[4];
            snprintf(hexDigit, sizeof(hexDigit), "%.2X", buf[i]);
            strcat(UID, hexDigit);
        }
        puts("");
        break;
    default:
        TagType = 0;
        break;
    }

    return TagType;
}

static void irisnfc_sig_handler(int sig)
{
    switch (sig) {
    case SIGHUP:
    case SIGTERM:
        // Bring down NFC before we die
        fprintf(stderr, "Stopping NFC connection...\n");
        done = 1;
        exit(0);
        break;
    }
}

/* Monitor NFC for devices */
int main(int argc, char** argv)
{
    int res, fd;
    char buf[256];
    char lastTag[128] = { 0 };

    // Turn ourselves into a daemon
    daemon(1, 1);

    // Catch signals so we can bring down connection
    signal(SIGHUP, irisnfc_sig_handler);
    signal(SIGTERM, irisnfc_sig_handler);

    // Setup syslog
    openlog(SYSLOG_IDENT, (LOG_CONS | LOG_PID | LOG_PERROR), LOG_DAEMON);

    // Make sure NFC device is present
    if (0 != access(NFC_DEV, F_OK)) {
        syslog(LOG_ERR, "Cannot connect to NFC device - exiting!");
        exit(0);
    }

    // Initialize NFC
    nfc_init();

    // Check for tags
    while (!done) {

        // Tag found?
        if (nfc_getTag(buf)) {
            if ((buf[0] != '\0') &&
                (strncmp(buf, lastTag, sizeof(lastTag) - 1))) {

                // If last tag hasn't been processed, wait some more
                if (access(NFC_TAG_FILE, F_OK) == 0) {
                    continue;
                }

                syslog(LOG_ERR, "Found new NFC Tag: %s", buf);
                strncpy(lastTag, buf, sizeof(lastTag) - 1);

                // Dump tag to file
                snprintf(buf, sizeof(buf), "echo %s > %s", lastTag,
                         NFC_TAG_FILE);
                system(buf);

                // Play a tone to signal device found?
                //snprintf(buf, sizeof(buf), "play_tones -f %s",
                //         REBOOT_SOUND_FILE);
                //system(buf);
            }
        } else {
            lastTag[0] = '\0';
        }

        // Do this every so often
        sleep(NFC_CHECK_DELAY);
    }
    return 0;
}
