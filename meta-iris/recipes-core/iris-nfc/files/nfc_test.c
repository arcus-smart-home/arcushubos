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

#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <libst95hf.h>


static uint8_t mode = 0;
static uint8_t bits = 8;
static uint32_t speed = 1000000;
static sURI_Info url;

#define TAG_SEARCH_PERIOD  15
#define URI_INFO           "Sample URI data"
#define URI_MESSAGE        "lowes.com/iris"
#define URI_PROTO          "http://"
#define NFC_DEVICE         "/dev/spidev1.0"

static void print_usage(const char *prog)
{
}

static void pabort(const char *file, const char *s)
{
    print_usage(file);
    perror(s);
    exit(-1);
}

int main(int argc, char *argv[])
{
    int ret = 0;
    int fd;
    int8_t TagType = TRACK_NOTHING;
    bool TagDetected = false;
    bool terminal_msg_flag = true;
    uint8_t status = ERRORCODE_GENERIC;
    time_t endTime = time(NULL) + TAG_SEARCH_PERIOD;
    char *device;

    // Assume device if none given
    if (argc == 1) {
      device = NFC_DEVICE;
    } else {
        device = argv[1];
    }

    if (0 != access(device, F_OK)) {
        print_usage(argv[0]);
	fprintf(stderr, "ERROR: \"%s\" cannot found!\n\n", device);
	exit(-1);
    }

    fd = open(device, O_RDWR);
    if (fd < 0)
        pabort(argv[0], "can't open device");

    /*
     * spi mode
     */
    ret = ioctl(fd, SPI_IOC_WR_MODE, &mode);
    if (ret == -1)
        pabort(argv[0], "can't set spi mode");

    ret = ioctl(fd, SPI_IOC_RD_MODE, &mode);
    if (ret == -1)
        pabort(argv[0], "can't get spi mode");

    /*
     * bits per word
     */
    ret = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
    if (ret == -1)
        pabort(argv[0], "can't set bits per word");

    ret = ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
    if (ret == -1)
        pabort(argv[0], "can't get bits per word");

    /*
     * max speed hz
     */
    ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
    if (ret == -1)
        pabort(argv[0], "can't set max speed hz");

    ret = ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
    if (ret == -1)
        pabort(argv[0], "can't get max speed hz");
    close(fd);

    printf("spi mode: %d\n", mode);
    printf("bits per word: %d\n", bits);
    printf("max speed: %d Hz (%d KHz)\n", speed, speed/1000);

    /* Initialize NFC support */
    ConfigManager_HWInit();

    /* Detect tags until time runs out */
    while (time(NULL) < endTime) {
        devicemode = PCD;

	/* Scan to find if there is a tag */
	TagType = ConfigManager_TagHunting(TRACK_ALL);

	/* Stop hunting to avoid interaction issues */
	ConfigManager_Stop();

	switch(TagType) {
	case TRACK_NFCTYPE1:
	    if (terminal_msg_flag == true) {
	        TagDetected = true;
	        terminal_msg_flag = false ;
		/*---HT UI msg----------*/
		printf( "TRACK_NFCTYPE1 NFC tag detected nearby\n");
	    }
	    break;
	case TRACK_NFCTYPE2:
	    if (terminal_msg_flag == true) {
	        TagDetected = true;
	        terminal_msg_flag = false ;
		/*---HT UI msg----------*/
		printf("TRACK_NFCTYPE2 NFC tag detected nearby\n");
	    }
	    break;
	case TRACK_NFCTYPE3:
	    if (terminal_msg_flag == true) {
	        TagDetected = true;
	        terminal_msg_flag = false ;
		/*---HT UI msg----------*/
		printf("TRACK_NFCTYPE3 NFC tag detected nearby\n");
	    }
	    break;
	case TRACK_NFCTYPE4A:
	    if (terminal_msg_flag == true) {
	        TagDetected = true;
	        terminal_msg_flag = false ;
		/*---HT UI msg----------*/
		printf("TRACK_NFCTYPE4A NFC tag detected nearby\n");
	    }
	    break;
	case TRACK_NFCTYPE4B:
	    if (terminal_msg_flag == true) {
	        TagDetected = true;
	        terminal_msg_flag = false ;
		/*---HT UI msg----------*/
		printf("TRACK_NFCTYPE4B NFC tag detected nearby\n");
	    }
	    break;
	case TRACK_NFCTYPE5:
	    if (terminal_msg_flag == true) {
	        uint8_t Buffer[20];
		uint8_t WriteBuffer [4]={0xE1, 0x20, 0x40, 0x00};
		uint8_t Buffer2[20];

	        TagDetected = true;
	        terminal_msg_flag = false ;
		/*---HT UI msg----------*/
		printf("TRACK_NFCTYPE5 NFC tag detected nearby\n");

		// Get UID
		ISO15693_GetUID(Buffer);
		printf("Tag UID: ");
		for (int ret = 7; ret >= 0 ; ret--) {
		    printf("%.2X", Buffer[ret]);
		}
		puts("");

		/* Set up tag for writing */
		ISO15693_WriteSingleBlock(0x02,0x00,0x00,WriteBuffer,Buffer2);
		ISO15693_ReadSingleBlock(0x02,0x00,0x00,Buffer);
	    }
	    break;
	default:
	    TagDetected = false;
	    if (terminal_msg_flag == false) {
	        terminal_msg_flag = true ;
		/*---HT UI msg----------*/
		printf("Currently there is no NFC tag in the vicinity\n");
	    }
	    break;
	}
	usleep(300000);

	if (TagDetected == true) {
	    uint8_t Buffer[512];

	    TagDetected = false;
	    memset(Buffer, 0, sizeof(Buffer));

	    /* Fill the structure of the NDEF URI */
	    strcpy(url.Information,URI_INFO);
	    strcpy(url.protocol,URI_PROTO);
	    strcpy(url.URI_Message,URI_MESSAGE);
	    status = NDEF_WriteURI(&url);

	    // Wait for a moment before checking data
	    usleep(100000);

	    if (status == RESULTOK) { /*---if URI write passed----------*/
		status = ERRORCODE_GENERIC;
		printf("URI Information written successfully on the tag\n");

		/* Clear url buffer before reading */
		memset(url.Information,'\0',sizeof(url.Information));

		if (TagType == TRACK_NFCTYPE1) {
		    status = PCDNFCT1_ReadNDEF(Buffer);
		} else if (TagType == TRACK_NFCTYPE2) {
		    status = PCDNFCT2_ReadNDEF(Buffer);
		} else if (TagType == TRACK_NFCTYPE3) {
		    status = PCDNFCT3_ReadNDEF(Buffer);
		} else if (TagType == TRACK_NFCTYPE4A || TagType == TRACK_NFCTYPE4B) {
		    status = PCDNFCT4_ReadNDEF(Buffer);
		} else if (TagType == TRACK_NFCTYPE5) {
		    status = PCDNFCT5_ReadNDEF(Buffer);
		}

		if (status == RESULTOK) {
		    sRecordInfo RecordStruct;
		    status = NDEF_IdentifyBuffer( &RecordStruct, &Buffer[6]);
		    if (status == RESULTOK && RecordStruct.TypeLength != 0) {
		        if (NDEF_ReadURI(&RecordStruct, &url)==RESULTOK) {
			    /*---if URI read passed---*/
			    if ((strncmp((char *)url.Information,
					 URI_INFO, strlen(URI_INFO)) == 0) &&
				(strncmp((char *)url.protocol,
					 URI_PROTO, strlen(URI_PROTO)) == 0) &&
				(strncmp((char *)url.URI_Message,
					 URI_MESSAGE, strlen(URI_MESSAGE)) == 0)) {
			        printf("URI Information read successfully from the tag!\n");
				printf("PASSED!\n");
				return 0;
			    }
			}
		    }
		}
	    }
	}
    }

    /* If we get here, we have timed out */
    printf("FAILED!\n");
    return 1;
}


