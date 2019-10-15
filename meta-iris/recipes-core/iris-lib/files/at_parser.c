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
#include <sys/types.h>
#include <sys/select.h>
#include <termios.h>
#include <pthread.h>
#include "irislib.h"
#include "at_parser.h"

static pthread_mutex_t at_mutex = PTHREAD_MUTEX_INITIALIZER;

static char *atGetLine(int fd, int timeout)
{
    unsigned char newchar;
    fd_set set;
    struct timeval tv;
    int index = 0;
    char *line = malloc(256);

    // If no memory, return
    if (line == NULL) {
        return NULL;
    }

    do {
        // Setup what we are waiting on
        FD_ZERO(&set);
        FD_SET(fd, &set);
        tv.tv_sec = timeout;
        tv.tv_usec = 0;

        if (select(fd + 1, &set, NULL, NULL, &tv) >= 0) {
            if (read(fd, &newchar, 1) != 1) {
                fprintf(stderr, "Timeout reading AT response data!\n");
                free(line);
                return NULL;
            }
            line[index++] = newchar;
        } else {
            fprintf(stderr, "Error reading AT response data!\n");
            free(line);
            return NULL;
        }
    } while (newchar != '\n');

    // Terminate string
    line[index] = '\0';
    return line;
}

/* Open/close port */
ATPort *atOpen(char *port, int speed, int flow)
{
    int fd;
    ATPort *atp;

    // Allow only one user at a time to avoid odd problems!
    pthread_mutex_lock(&at_mutex);

    // Open serial port
    fd = open(port, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "Unable to open AT port (%s)\n", port);
        pthread_mutex_unlock(&at_mutex);
        return NULL;
    }

    // Set port settings
    IRIS_initSerialPort(fd, speed, flow);

    // Allocate struct and data buffer
    atp = malloc(sizeof(ATPort));

    if (atp == NULL) {
        close(fd);
        fprintf(stderr, "Unable to allocate AT port structure\n");
        pthread_mutex_unlock(&at_mutex);
        return NULL;
    }

    // Init struct
    atp->fd = fd;
    atp->state = IDLE;
    atp->status = OK;
    atp->data[0] = '\0';

    // Return port
    return atp;
}

void atClose(ATPort *port)
{
    // Nothing to do if no port
    if (port == NULL) return;

    // Close port
    close(port->fd);

    // Clean up
    free(port);

    // Unlock
    pthread_mutex_unlock(&at_mutex);
}


/* AT send */
int atSend(ATPort *port, char *cmd, int timeout)
{
    int done = 0, remaining = AT_DATA_BUF_SIZE;
    char *line;
    time_t endTime = time(NULL) + timeout;

    // Make sure we have a port and a command to send
    if ((port == NULL) || (cmd == NULL)) {
        return -1;
    }

    // Write command
    memset(port->data, 0, sizeof(port->data));
    if (write(port->fd, cmd, strlen(cmd)) != strlen(cmd)) {
        fprintf(stderr, "Unable to write AT command\n");
        port->status = ERROR;
        return -1;
    }

    // Go into data state as we no longer enable command echo
    port->state = DATA;

    // Get data
    while (!done) {
        // Get response line
        line = atGetLine(port->fd, timeout);

        //fprintf(stderr, "GOT LINE: %s\n", line);

        // Error?
        if ((line == NULL) || (time(NULL) > endTime)) {
            port->state = IDLE;
            port->status = ERROR;
            return -1;
        }

        // Skip blank lines
        if ((line[0] == '\n') || (line[0] == '\r')) {
            free(line);
            continue;
        }

        // Parse
        if (port->state == IDLE) {
            if (strncmp(line, "AT", 2) == 0) {
                port->state = DATA;
            }
        } else if (port->state == DATA) {
            if (strncmp(line, "OK", 2) == 0) {
                port->state = IDLE;
                port->status = OK;
                done = 1;
            } else if ((strncmp(line, "ERROR", 5) == 0) ||
                       (strncmp(line, "+CME ERROR", 10) == 0) ||
                       (strncmp(line, "+CMS ERROR", 10) == 0)) {
                port->state = IDLE;
                port->status = ERROR;
                done = 1;
            } else {
                // For now, just truncate if we get back too much data
                remaining -= strlen(line);
                if (remaining > 0) {
                    strcat(&port->data[0], line);
                }
            }
        }
        free(line);
    }
    return 0;
}

