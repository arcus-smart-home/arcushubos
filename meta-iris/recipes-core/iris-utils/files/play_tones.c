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
#include <ctype.h>
#include <irislib.h>
#include <sys/types.h>
#include <signal.h>


// Local defines
#define PWM_ON              1
#define PWM_OFF             0
#define DEFAULT_VOLUME      "100"
#define DEFAULT_DURATION    "1000"
#define FREQ_MIN            30.00
#define FREQ_MAX            20000.00
#define VOLUME_MIN          0
#define VOLUME_MAX          100
#define DURATION_MIN        10
#define DURATION_MAX        60000

typedef struct note_data {
    char   *note;
    double frequency;
} note_data_t;


// Local data

// Note data, rounded to whole numbers
static note_data_t notes[] = {
    { "C1",    32.70 },
    { "C#1",   34.65 },
    { "D1",    36.71 },
    { "D#1",   38.89 },
    { "E1",    41.20 },
    { "F1",    43.65 },
    { "F#1",   46.25 },
    { "G1",    49.00 },
    { "G#1",   51.91 },
    { "A1",    55.00 },
    { "A#1",   58.27 },
    { "B1",    61.74 },
    { "C2",    65.41 },
    { "C#2",   69.30 },
    { "D2",    73.42 },
    { "D#2",   77.78 },
    { "E2",    82.41 },
    { "F2",    87.31 },
    { "F#2",   92.50 },
    { "G2",    98.00 },
    { "G#2",   103.83 },
    { "A2",    110.00 },
    { "A#2",   116.54 },
    { "B2",    123.47 },
    { "C3",    130.81 },
    { "C#3",   138.59 },
    { "D3",    146.83 },
    { "D#3",   155.56 },
    { "E3",    164.81 },
    { "F3",    174.61 },
    { "F#3",   185.00 },
    { "G3",    196.00 },
    { "G#3",   207.65 },
    { "A3",    220.00 },
    { "A#3",   233.08 },
    { "B3",    246.94 },
    { "C4",    261.63 },
    { "C#4",   277.18 },
    { "D4",    293.66 },
    { "D#4",   311.13 },
    { "E4",    329.63 },
    { "F4",    349.23 },
    { "F#4",   369.99 },
    { "G4",    392.00 },
    { "G#4",   415.30 },
    { "A4",    440.00 },
    { "A#4",   466.16 },
    { "B4",    493.88 },
    { "C5",    523.25 },
    { "C#5",   554.37 },
    { "D5",    587.33 },
    { "D#5",   622.25 },
    { "E5",    659.26 },
    { "F5",    698.46 },
    { "F#5",   739.99 },
    { "G5",    783.99 },
    { "G#5",   830.61 },
    { "A5",    880.00 },
    { "A#5",   932.33 },
    { "B5",    987.77 },
    { "C6",    1046.50 },
    { "C#6",   1108.73 },
    { "D6",    1174.66 },
    { "D#6",   1244.51 },
    { "E6",    1318.51 },
    { "F6",    1396.91 },
    { "F#6",   1479.98 },
    { "G6",    1567.98 },
    { "G#6",   1661.22 },
    { "A6",    1760.00 },
    { "A#6",   1864.66 },
    { "B6",    1975.53 },
    { "C7",    2093.00 },
    { "C#7",   2217.46 },
    { "D7",    2349.32 },
    { "D#7",   2489.02 },
    { "E7",    2637.02 },
    { "F7",    2793.83 },
    { "F#7",   2959.96 },
    { "G7",    3135.96 },
    { "G#7",   3322.44 },
    { "A7",    3520.00 },
    { "A#7",   3729.31 },
    { "B7",    3951.07 },
    { "C8",    4186.01 },
    { "C#8",   4434.92 },
    { "D8",    4698.64 },
    { "D#8",   4978.03 },
    { "E8",    5274.04 },
    { "F8",    5587.65 },
    { "F#8",   5919.91 },
    { "G8",    6271.93 },
    { "G#8",   6644.88 },
    { "A8",    7040.00 },
    { "A#8",   7458.62 },
    { "B8",    7902.13 },
    { NULL,    0 }
};


static void usage(char *name)
{
    fprintf(stderr, "\nusage: %s [options] tone\n"
            "  options:\n"
            "    -v volume       Volume, 0 - 100 (default 100)\n"
            "    -d duration     Duration, in ms (default 1000 ms)\n"
            "    -f filename     File with tone data to replay\n"
            "    -h              Print this message\n"
            " tone               Tone (in Hz or symbol) to play\n"
            "\n", name);
}

#if defined(imxdimagic)
// Play sound via speaker
static int play_sound_via_speaker(int freq, int vol, long int duration)
{
    char cmd[128];

    // Set Volume - ignore for now as level is so low to begin with! FIXLATER
    snprintf(cmd, sizeof(cmd) - 1,
             "/usr/bin/amixer set Speaker 100%% > /dev/null 2>&1");
    //snprintf(cmd, sizeof(cmd) - 1,
    //       "/usr/bin/amixer set Speaker %d%% > /dev/null 2>&1", vol);
    system(cmd);

    // Play tone if volume is set
    if (vol != 0) {
        FILE *f;
        char line[256];
        pid_t pid;

        // Need a minimum delay for things to work (350 ms) due to command
        //  start up delay
        if (duration < 350) {
            duration = 350;
        }

        // Use "speaker-test" to play tone
        snprintf(cmd, sizeof(cmd) - 1,
                 "/usr/bin/speaker-test -t sine -f %d -s 1 > /dev/null 2>&1 &",
                 freq);
        system(cmd);

        // Get pid of command
        f = popen("pidof speaker-test", "r");
        fgets(line, sizeof(line), f);
        pid = strtoul(line, NULL, 10);
        pclose(f);

        // Kill after duration
        usleep(duration * 1000);
        kill(pid, SIGKILL);
    } else {
        // Just delay if nothing to play
        usleep(duration * 1000);
    }
    return 0;
}
#endif

// Set PWM period to control tone frequency
static int setPwmPeriod(unsigned long int period)
{
    FILE *f;
    char data[16];

    // Check for support first
    if (access(PWM_PERIOD, F_OK) == -1) {
        return -1;
    }

    f = fopen(PWM_PERIOD, "w");
    if (f != NULL) {
        snprintf(data, sizeof(data), "%lu\n", period);
        fwrite(data, 1, strlen(data), f);
        fclose(f);
        return 0;
    }
    return 1;
}

// Set PWM duty cycle to control volume
static int setPwmDutyCycle(unsigned long int dutyCycle)
{
    FILE *f;
    char data[32];

    // Check for support first
    if (access(PWM_DUTY_CYCLE, F_OK) == -1) {
        return -1;
    }

    f = fopen(PWM_DUTY_CYCLE, "w");
    if (f != NULL) {
        snprintf(data, sizeof(data), "%lu\n", dutyCycle);
        fwrite(data, 1, strlen(data), f);
        fclose(f);
        return 0;
    }
    return 1;
}

// Enable/disable PWM
static int setPwmEnable(int enable)
{
    FILE *f;
    char data[16];

    // Check for support first
    if (access(PWM_ENABLE, F_OK) == -1) {
        return -1;
    }

    f = fopen(PWM_ENABLE, "w");
    if (f != NULL) {
        snprintf(data, sizeof(data), "%d\n", enable);
        fwrite(data, 1, strlen(data), f);
        fclose(f);
        return 0;
    }
    return 1;
}

// Create PWM settings for tone
static int playTone(char *freq, char *volume, char *duration)
{
    int res = 0;
    long int volumeValue, delay;
    unsigned long int period, dutyCycle;
    double freqValue;

    // Convert frequency
    errno = 0;
    if (isalpha(freq[0])) {
        int i;

        // Check for note match
        for (i = 0, freqValue = 0.00; notes[i].note != NULL; i++) {
            if (strncmp(notes[i].note, freq, strlen(freq)) == 0) {
                freqValue = notes[i].frequency;
            }
        }
        if (freqValue == 0.00) {
            fprintf(stderr, "Unknown note value (%s)\n", freq);
            return 1;
        }
    } else {
        if (sscanf(freq, "%lf", &freqValue) != 1) {
            fprintf(stderr, "Error converting frequency value (%s)\n", freq);
            return 1;
        }
    }
    if ((freqValue < FREQ_MIN) || (freqValue > FREQ_MAX)) {
        fprintf(stderr, "Frequency (%f Hz) is out of range (%f - %f Hz)\n",
                freqValue, FREQ_MIN, FREQ_MAX);
        return 1;
    }

    // Convert volume
    volumeValue = strtol(volume, NULL, 10);
    if (errno) {
        fprintf(stderr, "Error converting volume value (%s)\n", volume);
        return 1;
    }
    if ((volumeValue < VOLUME_MIN) || (volumeValue > VOLUME_MAX)) {
        fprintf(stderr, "Volume (%ld) is out of range (%d - %d)\n",
                volumeValue, VOLUME_MIN, VOLUME_MAX);
        return 1;
    }

    // Convert duration
    delay = strtol(duration, NULL, 10);
    if (errno) {
        fprintf(stderr, "Error converting duration value (%s)\n", duration);
        return 1;
    }
    if ((delay < DURATION_MIN) || (delay > DURATION_MAX)) {
        fprintf(stderr, "Duration (%ld) is out of range (%d - %d ms)\n",
                delay, DURATION_MIN, DURATION_MAX);
        return 1;
    }

    // Reset PWM
    res = setPwmEnable(PWM_OFF);

#if defined(imxdimagic)
    // If we don't have a PWM enable, assume speaker is available
    if (res == -1) {
        return play_sound_via_speaker((int)freqValue, volumeValue, delay);
    }
#endif

    // Reset duty cycle
    res |= setPwmDutyCycle(0);

    // Calculate period from frequency (period is in ns)
    period = (long)((1000000000.00 / freqValue) + 0.5);
    res |= setPwmPeriod(period);

    // Calculate duty cycle as volume (again, in ns) - 50% is max volume
    dutyCycle = (period * volumeValue) / (2 * 100);
    res |= setPwmDutyCycle(dutyCycle);

    // Turn on sound
    res |= setPwmEnable(PWM_ON);

    // If we got an error setting data, return before delay
    if (res) {
        return res;
    }

    // Play note for selected duration (in ms)
    usleep(delay * 1000);

    return 0;
}

// Play a tone. or series of tones, using the piezo sounder
int main(int argc, char** argv)
{
    int  c, res = 0;
    char *tonearg = NULL, *volarg = NULL, *durarg = NULL, *filearg = NULL;

    // Parse options...
    opterr = 0;
    while ((c = getopt(argc, argv, "hv:d:f:")) != -1)
    switch (c) {
    case 'h':
        usage(argv[0]);
        return 0;
    case 'v':
        volarg = optarg;
        break;
    case 'd':
        durarg = optarg;
        break;
    case 'f':
        filearg = optarg;
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

    tonearg = argv[optind];

    // If a file is present, ignore other options
    if (filearg != NULL) {
        FILE *f;
        char line[128];

        f = fopen(filearg, "r");
        if (f == NULL) {
            fprintf(stderr, "Unable to open %s to get tone data!\n", filearg);
            exit(1);
        }

        // Play each line of tone file
        while (!res && (fgets(line, sizeof(line), f) != NULL)) {
            char tone[16];
            char volume[16];
            char duration[16];

            // Comment or blank line?  Skip
            if ((line[0] == '#') || (line[0] == '\n')) continue;

            // Parse data
            if (sscanf(line, "%[^','],%[^','],%s",
                       tone, volume, duration) != 3) {
                fprintf(stderr, "Error parsing tone data file (%s)\n", line);
                exit(1);
            }

            // If volume has been set, use as "master volume"
            if (volarg != NULL) {
                int master = atoi(volarg);
                int level = atoi(volume);
                snprintf(volume, sizeof(volume), "%d", (level * master) / 100);
            }

            // Play tone
            res = playTone(tone, volume, duration);
        }
        fclose(f);
    } else {

        // Tone must be given!
        if (tonearg == NULL) {
            fprintf(stderr, "Tone frequency was not given - exiting\n");
            exit(1);
        }

        // Use defaults for other parameters if not given
        if (volarg == NULL) {
            volarg = DEFAULT_VOLUME;
        }
        if (durarg == NULL) {
            durarg = DEFAULT_DURATION;
        }

        // Play tone
        res = playTone(tonearg, volarg, durarg);
    }

    // Disable tone before exiting
    setPwmEnable(PWM_OFF);

    return res;
}
