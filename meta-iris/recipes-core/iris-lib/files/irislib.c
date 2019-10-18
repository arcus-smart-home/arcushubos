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
#include <pthread.h>
#ifndef __APPLE__
#include <sys/sendfile.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#if imxdimagic
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#endif
#include "irislib.h"

#if imxdimagic
// Support for v3 hub power controller for hub shutdown
#define SY6991_DEVICE_NAME      "/dev/i2c-1"
#define SY6991_SLAVE_ADDR       0x6A
#endif


//
//  Iris Library routines
//

int IRIS_isReleaseImage(void)
{
    // Check for presense of Java support to determine if release image
    if (access(JAVA_BIN_FILE, F_OK) != -1) {
        return 1;
    }
    return 0;
}

int IRIS_isMfgImage(void)
{
    // Serial number support is only in the mfg image
    if (access(SERIALIZE_BIN_FILE, F_OK) != -1) {
        return 1;
    }
    return 0;
}

int IRIS_isDevImage(void)
{
    // Do we lack java and serial number supporti
    if ((access(SERIALIZE_BIN_FILE, F_OK) == -1) &&
        (access(JAVA_BIN_FILE, F_OK) == -1)) {
        return 1;
    }
    return 0;
}

int IRIS_isSSHEnabled(void)
{
    // Look for SSH pid file
    if (access(SSH_PID_FILE, F_OK) != -1) {
        return 1;
    }
    return 0;
}

int IRIS_isSSHKeyGen(void)
{
    // Does the SSH key file exist?  If not, we are generating one at boot
    if (access(SSH_KEY_FILE, F_OK) == -1) {
        return 1;
    }
    return 0;
}

// Return current hardware version
int IRIS_getHardwareVersion(void)
{
    FILE *f;

    f = fopen(HW_VER_FILE, "r");
    if (f != NULL) {
        int value = 0;

        fscanf(f, "%d", &value);
        fclose(f);
        return value;
    }
    return 0;
}

// Get firmware version
int IRIS_getFirmwareVersion(char *version)
{
    FILE *f;

    f = fopen(VERSION_FILE, "r");
    if (f != NULL) {
        fscanf(f, "%s", version);
        fclose(f);
        return 1;
    }
    version[0] = '\0';
    return 0;
}

// Get hardware model
int IRIS_getModel(char *model)
{
    FILE *f;

    f = fopen(MODEL_FILE, "r");
    if (f != NULL) {
        fscanf(f, "%s", model);
        fclose(f);
        return 1;
    }
    model[0] = '\0';
    return 0;
}

// Get HubID
int IRIS_getHubID(char *hubID)
{
    FILE *f;

    f = fopen(HUB_ID_FILE, "r");
    if (f != NULL) {
        fscanf(f, "%s", hubID);
        fclose(f);
        return 1;
    }
    hubID[0] = '\0';
    return 0;
}

// Get MAC Address #1
int IRIS_getMACAddr1(char *macAddr)
{
    FILE *f;

    f = fopen(MAC_ADDR1_FILE, "r");
    if (f != NULL) {
        fscanf(f, "%s", macAddr);
        fclose(f);
        return 1;
    }
    macAddr[0] = '\0';
    return 0;
}

// Get MAC Address #2
int IRIS_getMACAddr2(char *macAddr)
{
    FILE *f;

    f = fopen(MAC_ADDR2_FILE, "r");
    if (f != NULL) {
        fscanf(f, "%s", macAddr);
        fclose(f);
        return 1;
    }
    macAddr[0] = '\0';
    return 0;
}

// Does the hardware support flow control on UARTS ports 2 and 4?
int IRIS_isHwFlowControlSupported(void)
{
    char model[32];

    // Newer models all support hardware flow control
    if (IRIS_getModel(model) && (strncmp(model, "IH200", 5))) {
        return 1;
    }

    // Check for version with hardware flow control support
    if (IRIS_getHardwareVersion() >= HW_VERSION_WITH_FLOWCONTROL) {
        return 1;
    }
    return 0;
}

// Is battery voltage detection supported by the hardware?
int IRIS_isBatteryVoltageDetectionSupported(void)
{
    char model[32];

    // Newer models all support this
    if (IRIS_getModel(model) && (strncmp(model, "IH200", 5))) {
        return 1;
    }

    // Check for version with battery voltage detection support
    if (IRIS_getHardwareVersion() >= HW_VERSION_WITH_BATTERY_VOLTAGE) {
        return 1;
    }
    return 0;
}

// Is LTE modem connected?
int IRIS_isLTEConnected(void)
{
    // Check for LTE control file
    if (access(LTE_CONTROL_FILE, F_OK) == 0) {
        FILE *f;

        f = fopen(LTE_CONTROL_FILE, "r");
        if (f != NULL) {
            char data[128];

            // Read data from lteControl file
            memset(data, 0, sizeof(data));
            fread(data, 1, sizeof(data)-1, f);
            fclose(f);

            // Removing trailing \n
            if (data[strlen(data) - 1] == '\n') {
                data[strlen(data) - 1] = '\0';
            }

            // Connected?
            if (strncmp(data, "connect", 7) == 0) {
                return 1;
            }
        }
    }
    return 0;
}

// Set LED value and delay a bit for it to be applied
void IRIS_setLedMode(char *mode)
{
    FILE *f;

    f = fopen(LED_MODE_FILE, "w");
    if (f != NULL) {
        fwrite(mode, 1, strlen(mode), f);
        fclose(f);
    }
    usleep(10000);
}

// Get current LED mode value
void IRIS_getLedMode(char *mode, size_t size)
{
    FILE *f;

    f = fopen(LED_MODE_FILE, "r");
    if (f != NULL) {
        fgets(mode, size, f);
        fclose(f);
    }
}

#ifndef __APPLE__
// Copy file routine - we can't call "cp" via system as this needs
//   to run as the agent user ...
int IRIS_copyFile(const char *src, const char *dst)
{
    int         fdSrc, fdDst;
    off_t       bytes = 0;
    struct stat fInfo;
    int         res;

    /* Open files */
    if ((src == NULL) || ((fdSrc = open(src, O_RDONLY, 0644)) == -1)) {
        return -1;
    }
    if ((dst == NULL) || ((fdDst = open(dst, O_RDWR | O_CREAT, 0755)) == -1)) {
        close(fdSrc);
        return -1;
    }

    /* Get file size */
    fstat(fdSrc, &fInfo);

    /* Use sendfile to copy data */
    res = sendfile(fdDst, fdSrc, &bytes, fInfo.st_size);
    close(fdSrc);
    close(fdDst);

    /* Was copy successful? */
    if (res == fInfo.st_size) {
        return 0;
    }
    return res;
}
#endif

/* Serial port setup */
void IRIS_initSerialPort(int fd, int baud, int mode) {
    struct termios tty;

    /* Get current settings */
    memset(&tty, 0, sizeof(tty));
    tcgetattr(fd, &tty);

    /* Set baud */
    cfsetospeed(&tty, baud);
    cfsetispeed(&tty, baud);

    /* Set N8, no parity */
    tty.c_cflag &= ~(PARENB | PARODD);
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE;
    tty.c_iflag &= ~IGNBRK;                         // disable break processing
    tty.c_cflag |= (CS8 | CLOCAL | CREAD);
    tty.c_lflag = 0;
    tty.c_oflag = 0;

    /* Enable hardware flow control? */
    if (mode != FLOW_CONTROL_NONE) {
        //
        // Input flags - Turn off input processing!  Needed to avoid
        //                data translation that leads to false FCS errors!
        //
        // convert break to null byte, no CR to NL translation,
        // no NL to CR translation, don't mark parity errors or breaks
        // no input parity check, don't strip high bit off,
        // no XON/XOFF software flow control
        tty.c_iflag &= ~(BRKINT | ISTRIP | INLCR | INPCK |
			 IGNCR | ICRNL | IMAXBEL | IXON | IXOFF | IXANY);

        if (mode == FLOW_CONTROL_XONXOFF) {
            tty.c_iflag |= IXON;                 // SW flow control

            // disable all control chars
            memset(tty.c_cc, _POSIX_VDISABLE, NCCS);
            tty.c_cc[VSTART] = CSTART;           // define XON and XOFF
            tty.c_cc[VSTOP] = CSTOP;
            tty.c_cflag &= ~CRTSCTS;
        } else {
            tty.c_cflag |= CRTSCTS;
        }
        tty.c_cc[VMIN]  = 1;                       // read doesn't block
        tty.c_cc[VTIME] = 0;                       // NO READ TIMEOUT!

    } else {
        tty.c_iflag &= ~(IXON | IXOFF | IXANY);    // shut off xon/xoff ctrl
        tty.c_cc[VMIN]  = 1;                       // read doesn't block
        tty.c_cc[VTIME] = 5;                       // 0.5 seconds read timeout
        tty.c_cflag &= ~CRTSCTS;
    }

    /* Apply settings and clear out any existing data */
    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        printf("Error when trying to set terminal settings\n");
    }
    tcflush(fd, TCOFLUSH);
}

void *playAudio(void *data)
{
    char cmd[256];
    char *filename = (char *)data;
    FILE *f;

    // If we are already playing a sound, don't allow another to be played
    snprintf(cmd, sizeof(cmd), "pidof %s", AUDIO_APP);
    f = popen(cmd, "r");
    if (f) {
        char line[128];

        // Is audio running?  If not, play new file
        if (fgets(line, sizeof(line), f) == NULL) {
            snprintf(cmd, sizeof(cmd), "%s %s > /dev/null 2>&1", AUDIO_APP,
                     filename);
            system(cmd);
        }
        pclose(f);
    }
    free(data);
    return NULL;
}

/* Play sound file */
void IRIS_playAudio(char *filename, int delay)
{
    if (access(filename, F_OK) != -1) {
        pthread_t audio_thread;

        // Run in another thread in case of crash
        if (pthread_create(&audio_thread, NULL, playAudio,
                           (void*)strdup(filename))) {
            return;
        }
        pthread_join(audio_thread, NULL);

        // Delay after sound plays
        if (delay) {
            usleep(delay);
        }
    }
}

/* Check interface state */
int IRIS_isIntfIPUp(char *intf)
{
    char cmd[256];
    FILE *f;

    snprintf(cmd, sizeof(cmd), CHECK_IF_IP_UP, intf);
    f = popen(cmd, "r");
    if (f) {
        char line[256];
        if (fgets(line, sizeof(line), f) != NULL) {
            pclose(f);
            return 1;
        }
        pclose(f);
    }
    return 0;
}

/* Power down hub, if possible */
void IRIS_powerDownHub(void)
{
#if imxdimagic
    int fd;
    unsigned char CHIP_6991[2] = {0x01,0x00}, CHIP_6991_0[2] = {4,0};

    // The v3 hub can be powered down when on battery
    fd = open(SY6991_DEVICE_NAME, O_RDWR);
    if (fd < 0) {
        printf("open %s failed/n", SY6991_DEVICE_NAME);
        return;
    }
    if (ioctl(fd, I2C_SLAVE_FORCE, SY6991_SLAVE_ADDR) < 0) {
        printf("ioctl: set slave address failed\n");
        close(fd);
        return;
    }

    // write Reg 0x04 to read version
    write(fd, CHIP_6991_0, 1);
    read(fd, CHIP_6991_0, 2);
    CHIP_6991_0[0] = 0x01;
    write(fd, CHIP_6991_0, 1);

    // read Reg 0x01
    if (read(fd, CHIP_6991, 2) >= 0) {
        // set OTG_Enable to 0
        CHIP_6991_0[0] = 0x01;
        CHIP_6991_0[1] = 0x84;
        write(fd, CHIP_6991_0, 2);
        printf("SY6991 disable OTG_ENABLE\n");
        read(fd, CHIP_6991, 2);
        printf("SY6991 read back value %02X\r\n", CHIP_6991[0]);
    } else {
        printf("SY6991 read failed!\n");
    }
    close(fd);

    // Turn off main power
    system("echo 106 > /sys/class/gpio/export");
    system("echo out > /sys/class/gpio/gpio106/direction");
    while (1) {
        system("echo 0 > /sys/class/gpio/gpio106/value");
    }
#endif
}

int IRIS_isIntfConnected(char *intf)
{
    char cmd[256];
    FILE *f;

    snprintf(cmd, sizeof(cmd), CHECK_IF_STATUS, intf);
    f = popen(cmd, "r");
    if (f) {
        char line[256];
        if (fgets(line, sizeof(line), f) != NULL) {
            pclose(f);
            return 1;
        }
        pclose(f);
    }
    return 0;
}



/* Watchdog support */
static void writeWatchdog(char *data) {
    char cmd[128];
    FILE *f;

    // If agent is using watchdog, no need to touch
    snprintf(cmd, sizeof(cmd), "/usr/bin/lsof | grep %s", HW_WATCHDOG_DEV);
    f = popen(cmd, "r");
    if ((f == NULL) || (fgets(cmd, sizeof(cmd)-1, f) == NULL)) {
	snprintf(cmd, sizeof(cmd), "echo %s > %s 2> /dev/null",
		 data, HW_WATCHDOG_DEV);
	system(cmd);
    }
    pclose(f);
}

void startWatchdog(void) {
    // Actually, writing any value will start the watchdog
    return writeWatchdog("1");
}

void pokeWatchdog(void) {
    return writeWatchdog("A");
}

void stopWatchdog(void) {
    // Use special value 'V' to stop the watchdog
    return writeWatchdog("V");
}
