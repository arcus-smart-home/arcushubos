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
#include <syslog.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <time.h>
#include <irislib.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

/* ioctl cmd */
#define MOTOR_MAGIC         'L'
#define IOCTL_GPIO_SPI_SET  _IOW(MOTOR_MAGIC, 0,int)
#define RING_DEV            "/dev/MBI6023"

// First version of hardware had 4 groups of LEDs rather than individual control
#define QUAD_1              1
#define QUAD_2              2
#define QUAD_3              3
#define QUAD_4              4

// Second spin of hardware added individual LED control
#define MIN_IND_LED_VER     2

// LED defines for all LEDs
#define ALL_LEDS            0xFF
#define LEDS_OFF            0x000000
#define TOTAL_LEDS          12
#define TOTAL_QUADS         4

// Mode defines
#define MAX_MODES                    12
#define MODE_SINGLE_BLINK_SHORT      1
#define MODE_SINGLE_BLINK_LONG       2
#define MODE_BLINK_SLOW              3
#define MODE_BLINK_FAST              4
#define MODE_TRIPLE_BLINK            5
#define MODE_SOLID                   6
#define MODE_ROTATE_SLOW             7
#define MODE_ROTATE_FAST             8
#define MODE_PULSE                   9
#define MODE_WAVE                   10
#define MODE_TICK_DOWN              11
#define MODE_TICK_UP                12

// Delay defines
#define DELAY_SLOW                  300000
#define DELAY_SLOW_SINGLE           100000
#define DELAY_BLINK                 300000
#define DELAY_FAST                  100000
#define DELAY_FAST_SINGLE           33333
#define DELAY_TRIPLE                500000

#define DELAY_TICK                  5
#define DELAY_1S                    1
#define DELAY_2S                    2

// To show segments in a clockwise manner, disable for counter-clockwise
#define CLOCKWISE           1

// LED struct depends on number of LEDs supported (12 individual or 4 groups)
typedef struct {
    int  mbi6023[15];
    int  index;
} GPIO_ATTR_ASTv1;

typedef struct {
    int  mbi6023[39];
    int  index;
} GPIO_ATTR_ASTv2;


static void usage(char *name)
{
    fprintf(stderr, "\nusage: %s [options]\n"
            "  options:\n"
            "    -h           Print this message\n"
            "    -r value     Set red brightness percentage (0-100, default 100)\n"
            "    -g value     Set green brightness percentage (0-100, default 100)\n"
            "    -b value     Set blue brightness percentage (0-100, default 100)\n"
            "    -c value     Set color as RGB percentage brightness hex value\n"
            "    -d value     Set duration (in seconds)\n"
            "    -m value     LED operation mode (1-9, default 6):\n"
            "                  1 -- Single Blink Once Short\n"
            "                  2 -- Single Blink Once Long\n"
            "                  3 -- Slow Blink Continuous\n"
            "                  4 -- Fast Blink Continuous\n"
            "                  5 -- Triple Blink Once\n"
            "                  6 -- Solid\n"
            "                  7 -- Rotate Slow Continuous\n"
            "                  8 -- Rotate Fast Continuous\n"
            "                  9 -- Pulse Continuous\n"
            "                  10 -- Wave Continuous\n"
            "                  11 -- Tick Down Continuous\n"
            "                  12 -- Tick Up Continuous\n"
            " Use ^C to stop continuous modes.\n"
            "\n", name);
}

static int writeRing(int fd, int color, int which)
{
    int r, g, b, hwVer;

    // Color value is in percentages of max brightness
    r = (((color & 0xFF0000) >> 16) * 0xFFFF) / 100;
    g = (((color & 0x00FF00) >> 8) * 0xFFFF) / 100;
    b = ((color & 0x0000FF) * 0xFFFF) / 100;

    // Check hardware version to see if individual LED support is present
    hwVer = IRIS_getHardwareVersion();

    // If version not set, assume latest as board is likely yet to be
    //  manufactured
    if (hwVer == 0) {
        hwVer = MIN_IND_LED_VER;
    }

    if (hwVer >= MIN_IND_LED_VER) {
        static GPIO_ATTR_ASTv2 ring;

        // Clear data to start
        memset(&ring, 0, sizeof(ring));

        // The first 3 values are the fixed header
        ring.mbi6023[0] = 0xFC00;
        ring.mbi6023[1] = 0xFC02;
        ring.mbi6023[2] = 0x9002;

        // Determine which LED to enable
        if ((which > 0) && (which <= TOTAL_LEDS)) {
            ring.mbi6023[((which - 1) * 3) + 3] = b;
            ring.mbi6023[((which - 1) * 3) + 4] = r;
            ring.mbi6023[((which - 1) * 3) + 5] = g;
        } else if (which == ALL_LEDS) {
            int i;
            for (i = 0; i < TOTAL_LEDS; i++) {
                ring.mbi6023[(i*3)+3] = b;
                ring.mbi6023[(i*3)+4] = r;
                ring.mbi6023[(i*3)+5] = g;
            }
        }
        ring.index = ARRAY_SIZE(ring.mbi6023);
        if (ioctl(fd, IOCTL_GPIO_SPI_SET, &ring) == -1) {
            return -1;
        }
    } else {
        static GPIO_ATTR_ASTv1 ring;

        // Clear data to start
        memset(&ring, 0, sizeof(ring));

        // The first 3 values are the fixed header
        ring.mbi6023[0] = 0xFC00;
        ring.mbi6023[1] = 0xFC00;
        ring.mbi6023[2] = 0x0000;

        // Determine which quadrant to enable
        if (which == QUAD_1) {
            ring.mbi6023[3] = b;
            ring.mbi6023[4] = r;
            ring.mbi6023[5] = g;
        } else if (which == QUAD_2) {
            ring.mbi6023[6] = b;
            ring.mbi6023[7] = r;
            ring.mbi6023[8] = g;
        } else if (which == QUAD_3) {
            ring.mbi6023[9] = b;
            ring.mbi6023[10] = r;
            ring.mbi6023[11] = g;
        } else if (which == QUAD_4) {
            ring.mbi6023[12] = b;
            ring.mbi6023[13] = r;
            ring.mbi6023[14] = g;
        } else if (which == ALL_LEDS) {
            int i;
            for (i = 0; i < TOTAL_QUADS; i++) {
                ring.mbi6023[(i*3)+3] = b;
                ring.mbi6023[(i*3)+4] = r;
                ring.mbi6023[(i*3)+5] = g;
            }
        }
        ring.index = ARRAY_SIZE(ring.mbi6023);
        if (ioctl(fd, IOCTL_GPIO_SPI_SET, &ring) == -1) {
            return -1;
        }
    }
    return 0;
}

static int writeTail(int fd, int color, int which)
{
    int r, g, b, tr, tg, tb;
    static GPIO_ATTR_ASTv2 ring;

    // Color value is in percentages of max brightness
    r = (((color & 0xFF0000) >> 16) * 0xFFFF) / 100;
    g = (((color & 0x00FF00) >> 8) * 0xFFFF) / 100;
    b = ((color & 0x0000FF) * 0xFFFF) / 100;
    tr = (((color & 0xFF0000) >> 16) * 0xFFFF) / 200;
    tg = (((color & 0x00FF00) >> 8) * 0xFFFF) / 200;
    tb = ((color & 0x0000FF) * 0xFFFF) / 200;

    // Clear data to start
    memset(&ring, 0, sizeof(ring));

    // The first 3 values are the fixed header
    ring.mbi6023[0] = 0xFC00;
    ring.mbi6023[1] = 0xFC02;
    ring.mbi6023[2] = 0x9002;

    // Determine which LED to enable
    if ((which > 0) && (which <= TOTAL_LEDS)) {
        ring.mbi6023[((which - 1) * 3) + 3] = b;
        ring.mbi6023[((which - 1) * 3) + 4] = r;
        ring.mbi6023[((which - 1) * 3) + 5] = g;
        // Light LED (dimmer) before and after to smooth transition
        if (which == 1) {
            ring.mbi6023[(11 * 3) + 3] = tb;
            ring.mbi6023[(11 * 3) + 4] = tr;
            ring.mbi6023[(11 * 3) + 5] = tg;
        } else {
            ring.mbi6023[((which - 2) * 3) + 3] = tb;
            ring.mbi6023[((which - 2) * 3) + 4] = tr;
            ring.mbi6023[((which - 2) * 3) + 5] = tg;
        }
        if (which == TOTAL_LEDS) {
            ring.mbi6023[(1 * 3) + 3] = tb;
            ring.mbi6023[(1 * 3) + 4] = tr;
            ring.mbi6023[(1 * 3) + 5] = tg;
        } else {
            ring.mbi6023[((which) * 3) + 3] = tb;
            ring.mbi6023[((which) * 3) + 4] = tr;
            ring.mbi6023[((which) * 3) + 5] = tg;
        }
    }
    ring.index = ARRAY_SIZE(ring.mbi6023);
    if (ioctl(fd, IOCTL_GPIO_SPI_SET, &ring) == -1) {
        return -1;
    }
    return 0;
}

static int writeSegments(int fd, int color, int which, int onOff)
{
    int r, g, b, index;
    static GPIO_ATTR_ASTv2 ring;

    // Color value is in percentages of max brightness
    r = (((color & 0xFF0000) >> 16) * 0xFFFF) / 100;
    g = (((color & 0x00FF00) >> 8) * 0xFFFF) / 100;
    b = ((color & 0x0000FF) * 0xFFFF) / 100;

    // Clear data to start
    memset(&ring, 0, sizeof(ring));

    // The first 3 values are the fixed header
    ring.mbi6023[0] = 0xFC00;
    ring.mbi6023[1] = 0xFC02;
    ring.mbi6023[2] = 0x9002;

    // Determine which LEDs to enable
    if (onOff) {
#ifdef CLOCKWISE
        for (index = 1; index <= which; index++) {
            ring.mbi6023[((TOTAL_LEDS - index) * 3) + 3] = b;
            ring.mbi6023[((TOTAL_LEDS - index) * 3) + 4] = r;
            ring.mbi6023[((TOTAL_LEDS - index) * 3) + 5] = g;
        }
#else
        for (index = 1; index <= which; index++) {
            ring.mbi6023[((index - 1) * 3) + 3] = b;
            ring.mbi6023[((index - 1) * 3) + 4] = r;
            ring.mbi6023[((index - 1) * 3) + 5] = g;
        }
#endif
    } else {
#ifdef CLOCKWISE
        for (index = which; index <= TOTAL_LEDS; index++) {
            ring.mbi6023[((TOTAL_LEDS - index) * 3) + 3] = b;
            ring.mbi6023[((TOTAL_LEDS - index) * 3) + 4] = r;
            ring.mbi6023[((TOTAL_LEDS - index) * 3) + 5] = g;
        }
#else
        for (index = which; index <= TOTAL_LEDS; index++) {
            ring.mbi6023[((index - 1) * 3) + 3] = b;
            ring.mbi6023[((index - 1) * 3) + 4] = r;
            ring.mbi6023[((index - 1) * 3) + 5] = g;
        }
#endif
    }
    ring.index = ARRAY_SIZE(ring.mbi6023);
    if (ioctl(fd, IOCTL_GPIO_SPI_SET, &ring) == -1) {
        return -1;
    }
    return 0;
}

void ringOff(int fd)
{
    // Turn ring off and update LED state so system knows we are done
    writeRing(fd, LEDS_OFF, ALL_LEDS);
    IRIS_setLedMode("all-off");
}

int main(int argc, char** argv)
{
    int fd;
    int color;
    int  c, hwVer;
    time_t endTime = 0;
    int  r = 100, g = 100, b = 100, m = 6, d = 0;
    int tmp_r, tmp_g, tmp_b, tmp_color, index;

    /* Parse options */
    opterr = 0;
    while ((c = getopt(argc, argv, "hr:g:b:m:c:d:")) != -1)
    {
        switch (c) {
        case 'h':
            usage(argv[0]);
            return 0;
        case 'r':
            r = strtoul(optarg, 0, 0);
            break;
        case 'g':
            g = strtoul(optarg, 0, 0);
            break;
        case 'b':
            b = strtoul(optarg, 0, 0);
            break;
        case 'd':
            d = strtoul(optarg, 0, 0);
            if (d > 0) {
                endTime = time(NULL) + d;
            }
            break;
        case 'c':
            color = strtoul(optarg, 0, 16);
            r = (color & 0x00FF0000) >> 16;
            g = (color & 0x0000FF00) >> 8;
            b = (color & 0x000000FF);
            break;
        case 'm':
            m = strtoul(optarg, 0, 0);
            if ((m < 1) || (m > MAX_MODES)) {
	        fprintf(stderr, "Illegal mode - values 1 through %d allowed!\n",
		      MAX_MODES);
                usage(argv[0]);
                return 1;
            }
            break;
        default:
            fprintf(stderr, "Error parsing options - exiting!\n");
            usage(argv[0]);
            return 1;
        }
    }

    // Force values within range
    if (r < 0) r = 0;
    if (r > 100) r = 100;
    if (g < 0) g = 0;
    if (g > 100) g = 100;
    if (b < 0) b = 0;
    if (b > 100) b = 100;

    fd = open(RING_DEV, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "Unable to open LED Ring!");
        return 0;
    }

    // Convert the individual percentages to a single value
    color = (r << 16) | (g << 8) | b;

    // Get hardware version - individual LED control not available in
    //  early versions of the hardware
    hwVer = IRIS_getHardwareVersion();

    // If version not set, assume latest as board is likely yet to be
    //  manufactured
    if (hwVer == 0) {
        hwVer = MIN_IND_LED_VER;
    }

    // Handle each mode
    switch (m) {

    case MODE_SINGLE_BLINK_SHORT:
        if (writeRing(fd, LEDS_OFF, ALL_LEDS)) {
            fprintf(stderr, "Error writing to LED ring!");
        }
        writeRing(fd, color, ALL_LEDS);
        usleep(DELAY_BLINK);
        ringOff(fd);
        break;

    case MODE_SINGLE_BLINK_LONG:
        if (writeRing(fd, LEDS_OFF, ALL_LEDS)) {
            fprintf(stderr, "Error writing to LED ring!");
        }
        writeRing(fd, color, ALL_LEDS);
        sleep(DELAY_2S);
        ringOff(fd);
        break;

    case MODE_BLINK_SLOW:
        while (1) {
            if (writeRing(fd, color, ALL_LEDS)) {
                fprintf(stderr, "Error writing to LED ring!");
            }
            sleep(DELAY_1S);
            writeRing(fd, LEDS_OFF, ALL_LEDS);
            sleep(DELAY_1S);

            // Done?
            if (endTime && (time(NULL) > endTime)) {
                ringOff(fd);
                break;
            }
        }
        break;

    case MODE_BLINK_FAST:
        while (1) {
            if (writeRing(fd, color, ALL_LEDS)) {
                fprintf(stderr, "Error writing to LED ring!");
            }
            usleep(DELAY_BLINK);
            writeRing(fd, LEDS_OFF, ALL_LEDS);
            usleep(DELAY_BLINK);

            // Done?
            if (endTime && (time(NULL) > endTime)) {
                ringOff(fd);
                break;
            }
        }
        break;

    case MODE_TRIPLE_BLINK:
        if (writeRing(fd, LEDS_OFF, ALL_LEDS)) {
            fprintf(stderr, "Error writing to LED ring!");
        }
        usleep(DELAY_TRIPLE);
        writeRing(fd, color, ALL_LEDS);
        usleep(DELAY_BLINK);
        writeRing(fd, LEDS_OFF, ALL_LEDS);
        usleep(DELAY_BLINK);
        writeRing(fd, color, ALL_LEDS);
        usleep(DELAY_BLINK);
        writeRing(fd, LEDS_OFF, ALL_LEDS);
        usleep(DELAY_BLINK);
        writeRing(fd, color, ALL_LEDS);
        usleep(DELAY_BLINK);
        writeRing(fd, LEDS_OFF, ALL_LEDS);
        usleep(DELAY_TRIPLE);
        ringOff(fd);
        break;

    case MODE_SOLID:
        // Solid LEDs
        if (writeRing(fd, color, ALL_LEDS)) {
            fprintf(stderr, "Error writing to LED ring!");
        }
        // Is there a duration?
        if (endTime) {
            while (1) {
                sleep(DELAY_1S);
                if (time(NULL) > endTime) {
                    ringOff(fd);
                    break;
                }
            }
        }
        break;

    case MODE_ROTATE_SLOW:
        while (1) {
            // Rotate around individual LEDs or quadrants of ring
            if (hwVer >= MIN_IND_LED_VER) {
                int i;
#ifdef CLOCKWISE
                for (i = TOTAL_LEDS; i > 0; i--) {
                    if (writeTail(fd, color, i)) {
                        fprintf(stderr, "Error writing to LED ring!");
                    }
                    usleep(DELAY_SLOW_SINGLE);
                }
#else
                for (i = 1; i <= TOTAL_LEDS; i++) {
                    if (writeTail(fd, color, i)) {
                        fprintf(stderr, "Error writing to LED ring!");
                    }
                    usleep(DELAY_SLOW_SINGLE);
                }
#endif
            } else {
                if (writeRing(fd, color, QUAD_1)) {
                    fprintf(stderr, "Error writing to LED ring!");
                }
                usleep(DELAY_SLOW);
                writeRing(fd, color, QUAD_2);
                usleep(DELAY_SLOW);
                writeRing(fd, color, QUAD_3);
                usleep(DELAY_SLOW);
                writeRing(fd, color, QUAD_4);
                usleep(DELAY_SLOW);
            }

            // Done?
            if (endTime && (time(NULL) > endTime)) {
                ringOff(fd);
                break;
            }
        }
        break;

    case MODE_ROTATE_FAST:
        while (1) {
            // Rotate around individual LEDs or quadrants of ring
            if (hwVer >= MIN_IND_LED_VER) {
                int i;
#ifdef CLOCKWISE
                for (i = TOTAL_LEDS; i > 0; i--) {
                    if (writeTail(fd, color, i)) {
                        fprintf(stderr, "Error writing to LED ring!");
                    }
                    usleep(DELAY_FAST_SINGLE);
                }
#else
                for (i = 1; i <= TOTAL_LEDS; i++) {
                    if (writeTail(fd, color, i)) {
                        fprintf(stderr, "Error writing to LED ring!");
                    }
                    usleep(DELAY_FAST_SINGLE);
                }
#endif
            } else {
                if (writeRing(fd, color, QUAD_1)) {
                    fprintf(stderr, "Error writing to LED ring!");
                }
                usleep(DELAY_FAST);
                writeRing(fd, color, QUAD_2);
                usleep(DELAY_FAST);
                writeRing(fd, color, QUAD_3);
                usleep(DELAY_FAST);
                writeRing(fd, color, QUAD_4);
                usleep(DELAY_FAST);
            }

            // Done?
            if (endTime && (time(NULL) > endTime)) {
                ringOff(fd);
                break;
            }
        }
        break;

    case MODE_PULSE:
        while (1) {
            for (index = 0; index <= 10; index++) {
                tmp_r = (r * index) / 10;
                tmp_g = (g * index) / 10;
                tmp_b = (b * index) / 10;
                tmp_color = (tmp_r << 16) | (tmp_g << 8) | tmp_b;
                if (writeRing(fd, tmp_color, ALL_LEDS)) {
                    fprintf(stderr, "Error writing to LED ring!");
                }
                usleep(DELAY_FAST);
            }

            // Done?
            if (endTime && (time(NULL) > endTime)) {
                ringOff(fd);
                break;
            }
        }
        break;

    case MODE_WAVE:
        while (1) {
            for (index = 0; index <= 10; index++) {
                tmp_r = (r * index) / 10;
                tmp_g = (g * index) / 10;
                tmp_b = (b * index) / 10;
                tmp_color = (tmp_r << 16) | (tmp_g << 8) | tmp_b;
                if (writeRing(fd, tmp_color, ALL_LEDS)) {
                    fprintf(stderr, "Error writing to LED ring!");
                }
                usleep(DELAY_FAST);
            }
            for (index = 10; index >= 0; index--) {
                tmp_r = (r * index) / 10;
                tmp_g = (g * index) / 10;
                tmp_b = (b * index) / 10;
                tmp_color = (tmp_r << 16) | (tmp_g << 8) | tmp_b;
                if (writeRing(fd, tmp_color, ALL_LEDS)) {
                    fprintf(stderr, "Error writing to LED ring!");
                }
                usleep(DELAY_FAST);
            }

            // Done?
            if (endTime && (time(NULL) > endTime)) {
                ringOff(fd);
                break;
            }
        }
        break;

    case MODE_TICK_DOWN:
        while (1) {
            for (index = 1; index <= TOTAL_LEDS; index++) {
                if (writeSegments(fd, color, index, 0)) {
                    fprintf(stderr, "Error writing to LED ring!");
                }
                // Has a duration been specified?
                if (d) {
                    usleep((d * 1000000) / TOTAL_LEDS);
                } else {
                    sleep(DELAY_TICK);
                }
            }

            // Done?
            if (endTime && (time(NULL) >= endTime)) {
                ringOff(fd);
                break;
            }
        }
        break;

    case MODE_TICK_UP:
        while (1) {
            for (index = 1; index <= TOTAL_LEDS; index++) {
                if (writeSegments(fd, color, index, 1)) {
                    fprintf(stderr, "Error writing to LED ring!");
                }
                // Has a duration been specified?
                if (d) {
                    usleep((d * 1000000) / TOTAL_LEDS);
                } else {
                    sleep(DELAY_TICK);
                }
            }

            // Done?
            if (endTime && (time(NULL) >= endTime)) {
                ringOff(fd);
                break;
            }
        }
        break;

    default:
      fprintf(stderr, "Unknown LED ring mode: %d\n", m);
      break;
    }
    close(fd);
}
