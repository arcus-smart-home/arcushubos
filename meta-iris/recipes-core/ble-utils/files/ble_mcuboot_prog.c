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


/******************************************************************************
#
#  This is partially based on code from the mcuboot go code
#  which carries the following copyright:
#
******************************************************************************/

/**
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */


#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <termios.h>
#include <time.h>
#include <cbor.h>
#include <irislib.h>
#include "base64.h"

/* ----------------------------------------------------------------------------
 *                                        Constants
 * ----------------------------------------------------------------------------
 */


#define RX_TIMEOUT                          10
#define MAX_BLE_RESET_ATTEMPTS              5

#define CRC_POLY                            0x1021
#define CRC_INIT                            0x0000

#define NMP_HDR_SIZE                        8
#define READ_CHUNK_SIZE                     196
#define MAX_BUF_SIZE                        512

#define NMP_OP_READ                         0
#define NMP_OP_READ_RSP                     1
#define NMP_OP_WRITE                        2
#define NMP_OP_WRITE_RSP                    3

#define NMP_GROUP_DEFAULT                   0
#define NMP_GROUP_IMAGE                     1

#define NMP_ID_IMAGE_UPLOAD                 1
#define NMP_ID_CONS_ECHO_CTRL               1

#define SHELL_NLIP_PKT_START1               6
#define SHELL_NLIP_PKT_START2               9

#define SHELL_NLIP_DATA_START1              4
#define SHELL_NLIP_DATA_START2              20

/* ----------------------------------------------------------------------------
 *                                           Typedefs
 * ----------------------------------------------------------------------------
 */


/* ---------------------------------------------------------------------------
 *                                     Local Variables
 * ---------------------------------------------------------------------------
 */

static unsigned char start[2] = { SHELL_NLIP_PKT_START1, SHELL_NLIP_PKT_START2 };
static unsigned char cont[2] = { SHELL_NLIP_DATA_START1, SHELL_NLIP_DATA_START2 };

static unsigned char ble_buf[MAX_BUF_SIZE];
static unsigned char ble_res[MAX_BUF_SIZE];

static unsigned char curSeq = 0;

/* Reset support in in main code */
void BLE_assertReset(void);
void BLE_deassertReset(void);


static unsigned short crc16(unsigned char *buf, int len, bool pad)
{
    unsigned short crc = CRC_INIT;
    size_t padding = (pad) ? sizeof(crc) : 0;
    size_t i, b;

    /* src length + padding (if required) */
    for (i = 0; i < len + padding; i++) {

        for (b = 0; b < 8; b++) {
            unsigned short divide = crc & 0x8000;

            crc = (crc << 1);

            /* choose input bytes or implicit trailing zeros */
            if (i < len) {
                crc |= !!(buf[i] & (0x80 >> b));
            }

            if (divide) {
                crc = crc ^ CRC_POLY;
            }
        }
    }
    return crc;
}

static int BLE_getByte(int fd, unsigned char* ch) {
    unsigned char newchar;
    fd_set set;
    struct timeval timeout;

    /* Setup what we are waiting on */
    FD_ZERO(&set);
    FD_SET(fd, &set);

    /* Timeout after a few seconds */
    timeout.tv_sec = RX_TIMEOUT/2;
    timeout.tv_usec = 0;

    if (select(fd + 1, &set, NULL, NULL, &timeout) > 0) {
        if (read(fd, &newchar, 1) == 1) {
            *ch = newchar;
            return 0;
        }
    }
    return -1;
}

// Get a line from BLE module
static int BLE_getLine(int fd, unsigned char *buf, size_t maxLen)
{
    unsigned char ch;
    int index = 0;

    // Messages end with a newline
    while (BLE_getByte(fd, &ch) == 0) {
        if (ch == '\n') {
            return index;
        }
        buf[index++] = ch;
        if (index == maxLen) {
            return maxLen;
        }
    }
    return 0;
}

static int BLE_txRaw(int fd, unsigned char *buf, int len)
{
    if (write(fd, buf, len) != len) {
        return -1;
    }
    return 0;
}


static int BLE_tx(int fd, unsigned char *buf, int len)
{
    unsigned short crc;
    int i, written;
    unsigned char encoded[MAX_BUF_SIZE];
    size_t encoded_len;

    // Add length - account for CRC bytes as well
    ble_buf[0] = ((len + 2) & 0xFF00) >> 8;
    ble_buf[1] = (len + 2) & 0x00FF;

    // Add passed in data
    for (i = 0; i < len; i++) {
        ble_buf[2 + i] = buf[i];
    }

    // Add CRC
    crc = crc16(buf, len, true);
    ble_buf[2 + len] = (crc & 0xFF00) >> 8;
    ble_buf[2 + len + 1] = crc & 0x00FF;

    // Encode buffer
    if (base64_encode(encoded, sizeof(encoded), &encoded_len, ble_buf,
                      len + 4)) {
        return -1;
    }

    // Write in chunks
    written = 0;
    while (written < encoded_len) {
        int writeLen;

        /* write the packet stat designators. They are
         * different whether we are starting a new packet or continuing one */
        if (written == 0)  {
            BLE_txRaw(fd, start, sizeof(start));
        } else {
            /* slower platforms take some time to process each segment
             * and have very small receive buffers.  Give them a bit of
             * time here */
            usleep(20000);
            BLE_txRaw(fd, cont, sizeof(cont));
        }

        /* ensure that the total frame fits into 128 bytes.
         * base 64 is 3 ascii to 4 base 64 byte encoding.  so
         * the number below should be a multiple of 4.  Also,
         * we need to save room for the header (2 byte) and
         * carriage return (and possibly LF 2 bytes), */

        /* all totaled, 124 bytes should work */
        writeLen = (124 < (encoded_len - written)) ? 124 :
          (encoded_len - written);

        // Write packet data
        if (BLE_txRaw(fd, &encoded[written], writeLen)) {
            return -1;
        }

        // Terminate with newline
        BLE_txRaw(fd, (unsigned char *)"\n", 1);

        written += writeLen;
    }
    return 0;
}

// Blocking receive.
static unsigned char *BLE_getMsg(int fd, int *retLen)
{
    time_t endTime = time(NULL) + RX_TIMEOUT;

    // Get data
    while (time(NULL) < endTime) {
        size_t dataLen, decodedLen, totalLen = 0;
        int index = 0;
        unsigned char *line;
        unsigned char decoded[MAX_BUF_SIZE];

        // Get line
        dataLen = BLE_getLine(fd, ble_res, sizeof(ble_res));

        //fprintf(stdout, "Got BLE response(%d): %s\n", dataLen, ble_res);

        // If no newline was found, or no data, try again
        if ((dataLen == 0) || (dataLen == sizeof(ble_res))) {
            continue;
        }

        if ((dataLen > 1) && (ble_res[0] == '\r')) {
            line = &ble_res[1];
            dataLen--;
        } else {
            line = &ble_res[0];
        }

        // Make sure there is a correct header
        if ((dataLen < 2) || (((line[0] != SHELL_NLIP_DATA_START1) ||
                               (line[1] != SHELL_NLIP_DATA_START2)) &&
                              ((line[0] != SHELL_NLIP_PKT_START1) ||
                               (line[1] != SHELL_NLIP_PKT_START2)))) {
            continue;
        }

        // Base64 decode data - remove first two and trailing \n from decode
        if (base64_decode(decoded, sizeof(decoded), &decodedLen, &line[2],
                          dataLen - 3)) {
            continue;
        }

        // Skip if no data
        if (decodedLen == 0) {
            continue;
        }

        // Start of packet?
        if ((line[0] == SHELL_NLIP_PKT_START1) &&
            (line[1] == SHELL_NLIP_PKT_START2)) {

            if (decodedLen < 2) {
                continue;
            }

            totalLen = decoded[0] << 8 | decoded[1];
            decodedLen -= 2;
            memcpy(ble_res, &decoded[2], decodedLen);
            index = decodedLen;
        } else {
            memcpy(&ble_res[index], &decoded[0], decodedLen);
            index += decodedLen;
        }

        // Check CRC
        if (index == totalLen) {
            if (crc16(ble_res, totalLen, true)) {
                *retLen = totalLen;
                fprintf(stderr, "Bad CRC! %X\n", crc16(ble_res, totalLen, true));
            } else {
              *retLen = totalLen;
            }
            return ble_res;
        }
    }

    // Timeout
    *retLen = 0;
    return NULL;
}


static unsigned char *BLE_txNmp(int fd, unsigned char *hdr,
                                unsigned char *buf, int len, int *resLen)
{
    unsigned char data[MAX_BUF_SIZE];

    // Header is always 8 bytes - add to front of data
    memcpy(data, hdr, NMP_HDR_SIZE);

    // Add in body, if present
    if (len) {
        memcpy(&data[NMP_HDR_SIZE], buf, len);
    }

    // Send message
    if (BLE_tx(fd, data, len + NMP_HDR_SIZE)) {
        *resLen = 0;
        return NULL;
    }

    // Return response
    return BLE_getMsg(fd, resLen);
}



//      hdr.Op = uint8(data[0])
//      hdr.Flags = uint8(data[1])
//      hdr.Len = binary.BigEndian.Uint16(data[2:4])
//      hdr.Group = binary.BigEndian.Uint16(data[4:6])
//      hdr.Seq = uint8(data[6])
//      hdr.Id = uint8(data[7])

void BLE_populateNmpHdr(unsigned char *hdr, unsigned char nmp_op,
                        unsigned short nmp_group, unsigned char nmp_id,
                        unsigned short len)
{
    // Populate header
    hdr[0] = nmp_op;
    hdr[1] = 0;
    hdr[2] = (len & 0xFF00) >> 8;
    hdr[3] = len & 0x00FF;
    hdr[4] = (nmp_group & 0xFF00) >> 8;
    hdr[5] = nmp_group & 0x00FF;
    hdr[6] = curSeq++;
    hdr[7] = nmp_id;
}


static int BLE_txImage(int fd, char *filename, int slot)
{
    size_t          remaining;
    size_t          readCount;
    int             offset, res;
    FILE            *f = NULL;
    struct stat     sb;
    size_t          fileSize;

    // Make sure we can open file
    f = fopen(filename, "r");
    if (f == NULL) {
        fprintf(stderr, "Unable to open firmware file (%s) - exiting!\n",
                filename);
        res = -1;
        goto exit;
    }

    // Get size of file
    if (stat(filename, &sb) == -1) {
        fprintf(stderr, "Unable to get size of init file - exiting!\n");
        res = -1;
        goto exit;
    }

    // Slot can be 0 or 1, force to 0 otherwise
    if ((slot < 0) || (slot > 1)) {
        slot = 0;
    }

    // Write out data from file
    remaining = fileSize = sb.st_size;
    offset = 0;
    while (remaining) {
        unsigned char hdr[NMP_HDR_SIZE];
        unsigned char body[MAX_BUF_SIZE];
        unsigned char data[READ_CHUNK_SIZE];
        int bodyLen, resLen;
        unsigned char *response;
        CborEncoder encoder, mapEncoder;

        // How much should we read?
        if (remaining < READ_CHUNK_SIZE) {
            readCount = remaining;
        } else {
            readCount = READ_CHUNK_SIZE;
        }

        // Flash is erased on first block
        if (offset == 0) {
            fprintf(stdout, "Erasing BLE chip...");
            fflush(stdout);
        } else if (offset == READ_CHUNK_SIZE) {
            fprintf(stdout, "Done\nWriting data to BLE chip\n");
            fflush(stdout);
        }

        // Read chunk from file
        if (fread(data, 1, readCount, f) != readCount) {
            fprintf(stderr, "Error reading BLE data!\n");
            res = -1;
            goto exit;
        }

        // Create CBOR data
        cbor_encoder_init(&encoder, body, sizeof(body), 0);
        cbor_encoder_create_map(&encoder, &mapEncoder, CborIndefiniteLength);
        cbor_encode_text_stringz(&mapEncoder, "off");
        cbor_encode_uint(&mapEncoder, offset);
        if (offset == 0) {
            // Set upper byte with slot value (really just 0 or 1)
            if (slot) {
                remaining |= (slot & 0x0F) << 24;
            }
            cbor_encode_text_stringz(&mapEncoder, "len");
            cbor_encode_uint(&mapEncoder, remaining);
            remaining &= 0x00FFFFFF;
        }
        cbor_encode_text_stringz(&mapEncoder, "data");
        cbor_encode_byte_string(&mapEncoder, data, readCount);
        cbor_encoder_close_container(&encoder, &mapEncoder);
        bodyLen = cbor_encoder_get_buffer_size (&encoder, body);

        // Create header
        BLE_populateNmpHdr(hdr, NMP_OP_WRITE, NMP_GROUP_IMAGE,
                           NMP_ID_IMAGE_UPLOAD, bodyLen);

        // Send data
        if ((response = BLE_txNmp(fd, hdr, body, bodyLen, &resLen)) != NULL) {
            CborParser parser;
            CborValue map;
            CborError err;

            // Check header data
            if ((response[0] != NMP_OP_WRITE_RSP) ||
                (response[4] != ((NMP_GROUP_IMAGE & 0xFF00) >> 8)) ||
                (response[5] != (NMP_GROUP_IMAGE & 0x00FF)) ||
                (response[7] != NMP_ID_IMAGE_UPLOAD)) {
                fprintf(stderr, "Unexpected header response data!\n");
                res = -1;
                goto exit;
            }

	    // Check CBOR data
            err = cbor_parser_init(&response[8], resLen-8, 0, &parser, &map);
            if (!err) {
              CborValue value;
              if (cbor_value_map_find_value(&map, "rc", &value) == 0) {
                  int result;
                  if (cbor_value_get_int(&value, &result) == 0) {
                      //fprintf(stdout, "RC: %d ", result);
                  }
              }

              if (cbor_value_map_find_value(&map, "off", &value) == 0) {
                  CborType type = cbor_value_get_type(&value);

                  if (type == 0) {
                      int result;
                      if (cbor_value_get_int(&value, &result) == 0) {
			//fprintf(stdout, "Off: %d\n", result);
                      }
                  }
              }
            } else {
                fprintf(stderr, "Error decoding CBOR data!\n");
            }

        } else {
            fprintf(stderr, "Failed to send firmware data packet!\n");
            res = -1;
            goto exit;
        }

        // Update address - go on to next block
        remaining -= readCount;
        offset += readCount;

        // Show status, after first chunk where we erase
        if (offset > READ_CHUNK_SIZE) {
            fprintf(stdout,
                    "\r  %3d / %3d [ %3d%% ] chunks sent, file is %d bytes",
                    (offset / READ_CHUNK_SIZE), (fileSize / READ_CHUNK_SIZE),
                    ((offset * 100) / fileSize), fileSize);
            fflush(stdout);
        }
    }
    fprintf(stdout, "\n");

    // Success
    res = 0;

 exit:
    if (f != NULL) fclose(f);
    return res;
}

static int BLE_ping(int fd)
{
    unsigned char hdr[NMP_HDR_SIZE];
    unsigned char body[1];
    int bodyLen, resLen, res;
    unsigned char *response;

    // Just need to send a header, no body
    bodyLen = 0;

    // Create header
    BLE_populateNmpHdr(hdr, NMP_OP_READ, NMP_GROUP_DEFAULT,
                       NMP_ID_CONS_ECHO_CTRL, bodyLen);

    // Send data
    if ((response = BLE_txNmp(fd, hdr, body, bodyLen, &resLen)) != NULL) {
        // Check header data
        if ((response[0] != NMP_OP_READ_RSP) ||
            (response[4] != ((NMP_GROUP_DEFAULT & 0xFF00) >> 8)) ||
            (response[5] != (NMP_GROUP_DEFAULT & 0x00FF)) ||
            (response[7] != NMP_ID_CONS_ECHO_CTRL)) {
            fprintf(stderr, "Unexpected echo header response data!\n");
            res = -1;
        } else {
            res = 0;
        }
    } else {
        fprintf(stderr, "Failed to send echo data packet!\n");
        res = -1;
    }

    return res;
}


int BLE_vendorProgram(int fd, char *filename, int slot)
{
    int res, retries = MAX_BLE_RESET_ATTEMPTS;

    // SBL runs without flow control
    IRIS_initSerialPort(fd, B115200, FLOW_CONTROL_NONE);

    fprintf(stdout, "Resetting BLE chip...");

    // Reset BLE module via GPIO
    while (retries-- > 0) {
        BLE_assertReset();
        BLE_deassertReset();

        // Wait for bootloader to be ready (will send "ready\r\n")
        BLE_getLine(fd, ble_res, sizeof(ble_res));
        if (strncmp((char *)ble_res, "ready", 5) == 0) {
            fprintf(stdout, "Done\n");
            break;
        }
        fprintf(stdout, "retry...");
    }
    if (retries <= 0) {
        fprintf(stderr, "No reset response from BLE chip!\n");
        res = -1;
        goto exit;
    }

    // Ping to check connection
    if (BLE_ping(fd) < 0) {
        fprintf(stderr, "No ping response from BLE chip!\n");
        res = -1;
        goto exit;
    }

    // Send image
    if (BLE_txImage(fd, filename, slot) < 0) {
        fprintf(stderr, "Error programming image to BLE chip!\n");
        res = -1;
        goto exit;
    }

    // Success!
    res = 0;

 exit:

    // Reset BLE module
    BLE_assertReset();
    BLE_deassertReset();

    return res;
}

