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
#include <signal.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <glib.h>
#include <pthread.h>
#include <sys/inotify.h>
#include <semaphore.h>
#include <irislib.h>
#include <stropts.h>
#include <linux/watchdog.h>


/* Check the button state every second after it has been pushed */
#define ALARM_WAIT  1

/* Syslog name */
#define SYSLOG_IDENT       "irisinitd"

/* Check SSH access every few seconds */
#define SSH_PERIODIC 5

/* Check ifplugd process every so often as well */
#define IFPLUGD_PERIODIC 15

/* Delay a bit before starting wifi config thread to avoid notification issues*/
#define WIFI_CFG_INIT_DELAY       10

/* Poke watchdog - for mfg build - has a 5 minute timeout, do half as often */
#if defined(imxdimagic)
// The imx6 watchdog has a max of 128 seconds...
#define WATCHDOG_PERIOD         120
#define WATCHDOG_POKE_PERIODIC  60
#else
#define WATCHDOG_PERIOD         300
#define WATCHDOG_POKE_PERIODIC  150
#endif

#ifdef imxdimagic
// Check wifi status every few seconds, and give up after 30 seconds
#define WIFI_STATUS_DELAY         2
#define MAX_WIFI_STATUS_CHECKS    (30 / WIFI_STATUS_DELAY)

// Check connection for up to 60 seconds
#define INTERNET_CHECK_PERIOD     60

// Delay between sounds (in us)
#define AUDIO_SHORT_DELAY         500000

// Delay after playing audio
#define AUDIO_PLAY_DELAY          2500000

// Wait for the hub to come up for a bit before starting provisioning
#define PROV_INIT_DELAY           15

// How long we try to redo BLE provisioning
#define BLE_RESTART_PERIOD        300

// How long the Iris button needs to be held to reenter BLE provisioning (in ms)
#define BLE_REENTER_PERIOD        5000

// How often to do a wifi scan (in seconds)
#define WIFI_SCAN_PERIOD          60

// Audio files - part of agent install for now
#define WIFI_SUCCESS_VOICE  "/data/agent/conf/voice/ConnectedWiFi.mp3"
#define WIFI_FAILURE_VOICE  "/data/agent/conf/voice/WiFiConnectIssue.mp3"
#define INET_SUCCESS_VOICE  "/data/agent/conf/voice/GreatNewsInternetConnection.mp3"
#define INET_FAILURE_VOICE  "/data/agent/conf/voice/internet-failure.mp3"
#define BTN_NO_PLACE_VOICE  "/data/agent/conf/voice/buttonPressNoPlace.mp3"
#define FACTORY_RESET_VOICE "/data/agent/conf/voice/FactoryResetOffline.mp3"
#define REBOOT_VOICE        "/data/agent/conf/voice/button-reboot.mp3"
#define TURNING_OFF_VOICE   "/data/agent/conf/voice/Turningoff.mp3"
#define FIRST_BOOTUP_VOICE  "/data/agent/conf/voice/first-bootup.mp3"
#define BTN_PROV_IN_PROCESS "/data/agent/conf/voice/button-press-wi-fi-reconnect.mp3"

// Sounds as well
#define FAILURE_SOUND       "/data/agent/conf/sounds/failure.mp3"
#define SUCCESS_SOUND       "/data/agent/conf/sounds/Success.mp3"
#endif

/* Temp file for hubID */
#define TEMP_HUB_ID        "/tmp/hubID.txt"

/* Radio firmware defines */
#define FIRMWARE_DIR             "/data/firmware/"
#define ROOT_FW_DIR              "/home/root/fw/"
#define ZIGBEE_FIRMWARE          "zigbee-firmware-hwflow.bin"

static GIOChannel* buttonChannel;
static char last_led_mode[128] = { "boot-linux" };
static sem_t led_sem;
static int ssh_access = -1;
static int button_push = 0;
static gboolean reboot_sound_played = FALSE;
static gboolean factory_resetting = FALSE;
#ifdef imxdimagic
static long long startTime = 0;
static pthread_t bleProv_thread = 0;

// Wifi status
static WifiStatusType lastWifiStatus;
static char *wifiStatusStrs[] = {
    [DISCONNECTED] = "DISCONNECTED",
    [CONNECTING] = "CONNECTING",
    [BAD_SSID] = "BAD_SSID",
    [BAD_PASS] = "BAD_PASS",
    [NO_INTERNET] = "NO_INTERNET",
    [NO_SERVER] = "NO_SERVER",
    [CONNECTED] = "CONNECTED"
};

// Provisioning support
int isProvisioned(char *which);
static void *provisioningThreadHandler(void *ptr);

// Power down button support - halt CPU if held for a few seconds
static time_t pwrDownTime = 0;
#define POWER_DOWN_PERIOD   10
#endif

/* Set LED mode file */
static void setLedMode(char *mode)
{
    char cmd[128];

    // Don't add a new line to the data in this file!
    snprintf(cmd, sizeof(cmd), "echo -n %s > %s", mode, LED_MODE_FILE);
    system(cmd);
}

/* Simple alarm handler - if button is still pushed, reboot system */
static void alarmHandler(int sig)
{
    GError *err = 0;
    gsize read_count = 0;
    gchar buf[16];
    char  cmd[128];

    /* Ignore alarm signal for now */
    signal(sig, SIG_IGN);

    /* Reset file and read GPIO data */
    g_io_channel_seek_position(buttonChannel, 0, G_SEEK_SET, 0);
    g_io_channel_read_chars(buttonChannel, buf, sizeof(buf) - 1,
                            &read_count, &err);

    /* Button still being held? */
    if ((read_count == 2) && (buf[0] == BUTTON_HELD_VALUE)) {
        char *ledMode;

        /* Reset alarm */
        button_push++;
        signal(SIGALRM, alarmHandler);
        alarm(ALARM_WAIT);

        /* Update LEDs as user continues to hold button */
        if (button_push < BUTTON_REBOOT_HOLD) {
            /* Do nothing for very short push */
            return;
        } else if ((button_push >= BUTTON_REBOOT_HOLD) &&
            (button_push < BUTTON_SOFT_RESET_HOLD)) {
            ledMode = "button-reboot";

            // Play reboot sound, once
            if (!reboot_sound_played) {
#ifdef beaglebone
                snprintf(cmd, sizeof(cmd), "play_tones -f %s",
                         REBOOT_SOUND_FILE);
                system(cmd);
#endif
                reboot_sound_played = TRUE;
#ifdef imxdimagic
                // Play voice file as well
                IRIS_playAudio(SUCCESS_SOUND, AUDIO_SHORT_DELAY);
                IRIS_playAudio(REBOOT_VOICE, 0);
#endif
	    }
        } else if ((button_push >= BUTTON_SOFT_RESET_HOLD) &&
            (button_push < BUTTON_FACTORY_DEFAULT_HOLD)) {
            ledMode = "button-soft-reset";
        } else {
            ledMode = "button-factory-default";

            // Play factory reset sound, once
            if (!factory_resetting) {
#ifdef beaglebone
                snprintf(cmd, sizeof(cmd), "play_tones -f %s",
                         RESET_SOUND_FILE);
                system(cmd);
#endif
                factory_resetting = TRUE;
	    }
        }
        setLedMode(ledMode);
        return;
    } else {
        FILE *f;

        /* Action depends on button push duration */
        if (button_push < BUTTON_REBOOT_HOLD) {
          /* Do nothing for short button push */
        } else if ((button_push >= BUTTON_REBOOT_HOLD) &&
            (button_push < BUTTON_SOFT_RESET_HOLD)) {
            syslog(LOG_ERR, "Restarting system due to button push!");
#ifdef imxdimagic
            // Leave the LEDs on for a few seconds before reboot
            system("hub_restart 5");
#else
            system("hub_restart now");
#endif
            return;
        } else if ((button_push >= BUTTON_SOFT_RESET_HOLD) &&
            (button_push < BUTTON_FACTORY_DEFAULT_HOLD)) {
            syslog(LOG_ERR, "Performing soft reset due to button push!");
            snprintf(cmd, sizeof(cmd), "touch %s", AGENT_SOFT_RESET);
            system(cmd);
            snprintf(cmd, sizeof(cmd), "chmod 666 %s", AGENT_SOFT_RESET);
            system(cmd);
#ifdef imxdimagic
            // Leave the LEDs on for a few seconds before reboot
            system("hub_restart 5");
#else
            system("hub_restart 1");
#endif
            return;
        } else {
            syslog(LOG_ERR, "Performing factory default due to button push!");
#ifdef imxdimagic
            // Play voice file as well
            IRIS_playAudio(SUCCESS_SOUND, AUDIO_SHORT_DELAY);
            IRIS_playAudio(FACTORY_RESET_VOICE, 0);

            // Leave the LEDs on for 15 seconds, so stall here to avoid
            //  a quicker disable of the LED ring...
            sleep(10);
            // Then turn on shutdown LEDs so ring isn't dark!
            setLedMode("shutdown");
#endif
            system("factory_default");
            return;
        }

        /* Ignore button push if shorter than required hold time */
        signal(SIGALRM, alarmHandler);

        /* Re-enable button GPIO interrupt */
        f = fopen(BUTTON_GPIO_EDGE_FILE, "w");
        if (f != NULL) {
            fprintf(f, BUTTON_DEFAULT_EDGE "\n");
            fclose(f);
        }
        button_push = 0;
    }
}

/* Simple button handler - set alarm to check button state in x seconds  */
static gboolean buttonHandler(GIOChannel *channel, GIOCondition cond,
			      gpointer data)
{
    GError *err = 0;
    gsize read_count = 0;
    gchar buf[16];
    FILE  *f;

    /* Reset file position and read to clear interrupt */
    g_io_channel_seek_position(channel, 0, G_SEEK_SET, 0);
    g_io_channel_read_chars(channel, buf, sizeof(buf) - 1,
			    &read_count, &err);

    /* Turn off button GPIO interrupts */
    f = fopen(BUTTON_GPIO_EDGE_FILE, "w");
    if (f != NULL) {
        fprintf(f, "none\n");
        fclose(f);
    }

    /* Set an alarm for x seconds to check button state */
    alarm(ALARM_WAIT);
    return TRUE;
}

#ifdef imxdimagic

/* Simple Iris button handler */
static gboolean irisButtonHandler(GIOChannel *channel, GIOCondition cond,
                              gpointer data)
{
    GError *err = 0;
    gsize read_count = 0;
    gchar buf[16];
    struct timeval tv;

    /* Reset file position and read to clear interrupt */
    g_io_channel_seek_position(channel, 0, G_SEEK_SET, 0);
    g_io_channel_read_chars(channel, buf, sizeof(buf) - 1,
                            &read_count, &err);

    /* Button still being held? */
    if ((read_count == 2) && (buf[0] == IRIS_BUTTON_HELD_VALUE)) {
        gettimeofday(&tv, NULL);
        startTime = tv.tv_sec * 1000 + (tv.tv_usec) / 1000;
    } else if ((read_count == 2) && (startTime != 0)) {
        char cmd[128];

        // How long has the button been down?
        gettimeofday(&tv, NULL);
        long long delta = (tv.tv_sec * 1000 + (tv.tv_usec) / 1000) - startTime;
        syslog(LOG_ERR, "IRIS button pushed for %lld milliseconds", delta);

        // Rerun BLE provisioning if button held more than n seconds!
        if ((delta > BLE_REENTER_PERIOD) && IRIS_isReleaseImage() &&
            (bleProv_thread == 0)) {
            int atInit = 0;

            // Make sure wifi is down to start so we can detect ethernet
            snprintf(cmd, sizeof(cmd), WIFI_STOP_CMD, WIFI_IF, WIFI_IF);
            system(cmd);
            pthread_create(&bleProv_thread, NULL, &provisioningThreadHandler,
                           (void *)atInit);
            return TRUE;
        }

        // Pass on to agent for processing (and let agent update it after read)
        snprintf(cmd, sizeof(cmd), "echo %lld > %s; chmod 666 %s",
                 delta, IRIS_BTN_PUSHED_FILE, IRIS_BTN_PUSHED_FILE);
        system(cmd);

        // If not provisioned yet, play voice
        if (access(PROVISIONED_FILE, F_OK) == -1) {
            // Voice depends on if BLE provisioning is running or not
            if (bleProv_thread != 0) {
                char currentMode[64];

                // Restore current LED mode after this interruption
                IRIS_getLedMode(currentMode, sizeof(currentMode));
                setLedMode("button-press-wifi-reconnect");
                IRIS_playAudio(BTN_PROV_IN_PROCESS, AUDIO_SHORT_DELAY);
                setLedMode(currentMode);
            } else {
                IRIS_playAudio(BTN_NO_PLACE_VOICE, 0);
            }
        }
    }
    return TRUE;
}

/* Power down button handler */
static gboolean pwrDownButtonHandler(GIOChannel *channel, GIOCondition cond,
				     gpointer data)
{
    GError *err = 0;
    gsize read_count = 0;
    gchar buf[16];

    /* Reset file position and read to clear interrupt */
    g_io_channel_seek_position(channel, 0, G_SEEK_SET, 0);
    g_io_channel_read_chars(channel, buf, sizeof(buf) - 1,
                            &read_count, &err);

    /* Only check if we are on battery power - will not be able to
       shutdown otherwise... */
    if (access(BTRY_ON_FILE, F_OK) == -1) {
        return TRUE;
    }

    /* Button still being held? */
    if ((read_count == 2) && (buf[0] == PWR_DOWN_BTN_HELD_VALUE)) {
        pwrDownTime = time(NULL);
    } else if ((read_count == 2) && (pwrDownTime != 0)) {

        // How long has the button been down?  More than X seconds, power down
        time_t delta = time(NULL) - pwrDownTime;
        if (delta >= POWER_DOWN_PERIOD) {
            syslog(LOG_ERR,
                   "Power down button pushed for %ld seconds - halting!",
                   delta);

            // Set LED pattern then wait (5 seconds in total)
            setLedMode("turning-off");
            // Play voice file as well
            IRIS_playAudio(SUCCESS_SOUND, AUDIO_SHORT_DELAY);
            IRIS_playAudio(TURNING_OFF_VOICE, 0);
            sleep(3);

            // Stop agent
            system("killall irisagentd");
            system("killall java");
            sleep(2);

            // Power down sub-systems
            system("echo 0 > " PWR_EN_ZIGBEE_VALUE_FILE);
            system("echo 0 > " PWR_EN_ZWAVE_VALUE_FILE);
            system("echo 0 > " PWR_EN_BLE_VALUE_FILE);
            system("echo 0 > " PWR_EN_LTE_VALUE_FILE);

            // Sync filesystem
            system("sync; sync");
            sleep(3);

            // Power down hub - will not return
            IRIS_powerDownHub();
        } else {
            pwrDownTime = 0;
        }
    }
    return TRUE;
}
#endif

#ifdef beaglebone
/* Simple USB over-current handler - creates a file to mark when
   condition started, removes file when over */
static gboolean usbOCHandler(GIOChannel *channel, GIOCondition cond,
                             gpointer data)
{
    GError *err = 0;
    gsize read_count = 0;
    gchar buf[16];
    char  cmd[128];

    /* Data is USB channel - either 0 or 1 */
    if (((int)data != 0) && ((int)data != 1)) {
        return FALSE;
    }

    /* Reset file position and read to clear interrupt */
    g_io_channel_seek_position(channel, 0, G_SEEK_SET, 0);
    g_io_channel_read_chars(channel, buf, sizeof(buf) - 1,
                            &read_count, &err);

    /* Did the condition start? - record starting time*/
    if (buf[0] == '0') {
        if (data == 0) {
            if (access(USB0_OC_ON_FILE, F_OK) == -1) {
                syslog(LOG_ERR, "USB over-current condition on USB0!");
                snprintf(cmd, sizeof(cmd), "date > %s", USB0_OC_ON_FILE);
                system(cmd);
            }
        } else {
            if (access(USB1_OC_ON_FILE, F_OK) == -1) {
                syslog(LOG_ERR, "USB over-current condition on USB1!");
                snprintf(cmd, sizeof(cmd), "date > %s", USB1_OC_ON_FILE);
                system(cmd);
            }
        }
    } else {
        /* Remove file when condition stops */
        if (data == 0) {
            syslog(LOG_ERR, "Over-current condition on USB0 has stopped!");
            remove(USB0_OC_ON_FILE);
        } else {
            syslog(LOG_ERR, "Over-current condition on USB1 has stopped!");
            remove(USB1_OC_ON_FILE);
        }
    }
    return TRUE;
}
#endif

/* Check if a debug dongle has been inserted with key to allow SSH access */
static gboolean sshCheckAccess(gpointer data)
{
    FILE *f;
    char hubID[32];
    char keyFile[64] = { 0 };
    char cmd[256];
    int  cur_access;

    // See if key generation is running - if so, try again later
    if (IRIS_isSSHKeyGen()) {
        return TRUE;
    }

    /* Look in media mount location to see if we can find a key file */
    chdir(MEDIA_MOUNT_DIR);

    /* If we don't know the ssh_access state, check for dropbear pid file */
    if (ssh_access == -1) {
        if (IRIS_isSSHEnabled()) {
            ssh_access = 1;

            /* Create debug file to signal agent we are in debug mode */
            snprintf(cmd, sizeof(cmd), "touch %s", DEBUG_ENABLED_FILE);
            system(cmd);
        } else {
            ssh_access = 0;
        }
    }

    snprintf(cmd, sizeof(cmd), "cat %s", HUB_ID_FILE);
    f = popen(cmd, "r");
    if (f) {
        fscanf(f, "%s", hubID);
    }
    pclose(f);

    if (hubID[0] != '\0') {
        /* Removing trailing \n */
        if (hubID[strlen(hubID)-1] == '\n') {
            hubID[strlen(hubID)-1] = '\0';
        }
        snprintf(keyFile, sizeof(keyFile), "%s.dbg", hubID);
    }

    cur_access = 0;
    if (keyFile[0] != '\0') {
        char filename[256];

        snprintf(cmd, sizeof(cmd),
                 "find . -name %s -maxdepth %d | grep -v \'/\\.\'",
                 keyFile, MAX_FIND_DEPTH);
        f = popen(cmd, "r");
        if (f) {
            while (fgets(filename, sizeof(filename), f) != NULL) {
		/* Removing trailing \n */
		if (filename[strlen(filename)-1] == '\n') {
		    filename[strlen(filename)-1] = '\0';
		}

                /* Verify key */
                snprintf(cmd, sizeof(cmd),
                       "openssl rsautl -verify -in %s -pubin -inkey %s -out %s",
                         filename, BUILD_PUBLIC_KEY, TEMP_HUB_ID);
                if (system(cmd)) {
                    continue;
                }

                /* Does the key match our Hub ID? */
                snprintf(cmd, sizeof(cmd), "diff %s %s", TEMP_HUB_ID,
                         HUB_ID_FILE);
                if (!system(cmd)) {
                    cur_access = 1;
                    break;
                }
            }
            unlink(TEMP_HUB_ID);
        }
        pclose(f);
    }

    /* For release image, check if agent cfg file has changed */
    if (IRIS_isReleaseImage() && (hubID[0] != '\0')) {
        char cfgFile[64] = { 0 };
        char filename[256];
        int fileFound = 0, restartAgent = 0, currentCfg = 0;

        snprintf(cfgFile, sizeof(cfgFile), "%s.cfg", hubID);
        snprintf(cmd, sizeof(cmd),
                 "find . -name %s -maxdepth %d | grep -v \'/\\.\'",
                 cfgFile, MAX_FIND_DEPTH);
        f = popen(cmd, "r");

        /* Should just be one config file to avoid confusion */
        if (f) {
            if (fgets(filename, sizeof(filename), f) != NULL) {
                /* Removing trailing \n */
                if (filename[strlen(filename)-1] == '\n') {
                    filename[strlen(filename)-1] = '\0';
                }
                fileFound = 1;
            }
            pclose(f);
        }

        /* Current config? */
        if (access(AGENT_CFG_FILE, F_OK) != -1) {
            currentCfg = 1;
        }

        /* Copy over configuration file if it doesn't exist */
        if (fileFound && !currentCfg) {
            snprintf(cmd, sizeof(cmd), "cp %s %s", filename,
                     AGENT_CFG_FILE);
            system(cmd);
            restartAgent = 1;
        } else if (fileFound) {
            /* Has configuration file changed? */
            snprintf(cmd, sizeof(cmd), "cmp -s %s %s", filename,
                     AGENT_CFG_FILE);
            if (system(cmd)) {
                snprintf(cmd, sizeof(cmd), "cp %s %s", filename,
                         AGENT_CFG_FILE);
                system(cmd);
                restartAgent = 1;
            }
        } else if (currentCfg) {
            /* Configuration has been removed */
            unlink(AGENT_CFG_FILE);
            restartAgent = 1;
        }

        /* Restart agent if necessary */
        if (restartAgent) {
            syslog(LOG_ERR, "Restarting Hub Agent due to new configuration.");
            /* If we kill the agent, the watchdog will restart it */
            system("killall java");
        }
    }

    /* Allow access? */
    if (cur_access && !ssh_access) {
        syslog(LOG_ERR, "Enabling SSH access.");
        ssh_access = 1;

        /* Change permissions on config so agent can remove on factory reset! */
        snprintf(cmd, sizeof(cmd), "chmod 777 %s", DROPBEAR_CONFIG_DIR);
        system(cmd);

        /* Create debug file to signal agent we are in debug mode */
        snprintf(cmd, sizeof(cmd), "touch %s", DEBUG_ENABLED_FILE);
        system(cmd);

        /* Start SSH if it's not already running */
        if (!IRIS_isSSHEnabled()) {
            snprintf(cmd, sizeof(cmd), "%s realstart", SSH_INIT_SCRIPT);
            system(cmd);
        }
    } else if (!cur_access && ssh_access) {
        syslog(LOG_ERR, "Disabling SSH access.");
        ssh_access = 0;

        /* Remove debug file to signal agent we are not in debug mode */
        remove(DEBUG_ENABLED_FILE);

        /* Stop SSH if it's already running */
        if (IRIS_isSSHEnabled()) {
            snprintf(cmd, sizeof(cmd), "%s stop", SSH_INIT_SCRIPT);
            system(cmd);
        }

        /* Kill any SSH connections that may be open */
        syslog(LOG_ERR, "Terminating open SSH connections.");
        snprintf(cmd, sizeof(cmd), "pidof %s", SSH_BIN_FILE);
        f = popen(cmd, "r");
        if (f) {
            char line[128];
            while (fgets(line, sizeof(line), f) != NULL) {
                pid_t pid = strtoul(line, NULL, 10);
                if (pid) {
                    syslog(LOG_ERR, "Killing SSH process (%d).", pid);
                    kill(pid, SIGTERM);
                }
            }
            pclose(f);
        }
    }

    return TRUE;
}

/* Check if ifplugd is still running... */
static gboolean ifplugdWatchdog(gpointer data)
{
    FILE *f;
    char cmd[256];
    char line[128];

    /* If ifplugd is not running, restart */
    snprintf(cmd, sizeof(cmd), "pidof %s", IFPLUGD_PROCESS_NAME);
    f = popen(cmd, "r");
    if (f) {
        /* Is ifplugd running? */
        if (fgets(line, sizeof(line), f) == NULL) {
            syslog(LOG_ERR, "Restarting ifplugd process...");

            /* Need to make sure udhcpc is not running before we restart */
            system(UDHCPC_TERMINATE);
            sleep(1);
            system(IFPLUGD_START_SCRIPT);
        }
        pclose(f);
    }
    return TRUE;
}

/* Poke watchdog */
static gboolean hwWatchdog(gpointer data)
{
    /* Keep the hardware watchdog from rebooting */
    pokeWatchdog();
    return TRUE;
}

/* Simple led handler thread - call "ledctrl" utility to handle
   updates to ledMode file */
static void *ledThreadHandler(void *ptr)
{
    FILE *f;
    int  fd;
    char buf[512];
    int  len;

    /* We want to be notified when the LED mode file is changed */
    fd = inotify_init();
    if (fd >= 0) {

        /* Add ledMode file to watch list */
        inotify_add_watch(fd, LED_MODE_FILE, IN_MODIFY);

	/* We are ready for file changes */
	sem_post(&led_sem);

        /* Keep looking for file modifications... */
        while (1) {

            /* Read inotify fd to get events */
            len = read(fd, buf, sizeof(buf));
            if (len >= 0) {
                int i;

                /* Parse events, looking for modify events ... */
                i = 0;
                while (i < len) {
                    struct inotify_event *event =
		      (struct inotify_event *)&buf[i];

                    /* Got a file modification event */
                    if (event->mask & IN_MODIFY) {

                        /* Open ledMode file */
                        f = fopen(LED_MODE_FILE, "r");
                        if (f != NULL) {
                            char data[128];
                            char cmd[128];

                            /* Read data from ledMode file */
                            memset(data, 0, sizeof(data));
                            if (fread(data, 1, sizeof(data)-1, f) <= 0) {
                                /* Try again */
                                i += (sizeof(struct inotify_event)) +
                                  event->len;

                                /* Close ledMode file */
                                fclose(f);
                                continue;
                            }

                            /* Removing trailing \n */
                            if (data[strlen(data) - 1] == '\n') {
                                data[strlen(data) - 1] = '\0';
                            }

                            /* Update LED mode */
                            snprintf(cmd, sizeof(cmd) - 1, "%s %s",
				     LED_CTRL_APP, data);
                            system(cmd);

                            /* Close ledMode file */
                            fclose(f);

                            /* Handle error cases - need to flash LEDs for a
			       short period and perhaps reboot afterwards */
                            if (strstr(data, "-err") != NULL) {

                                /* Errors during bootloader, kernel or rootfs
                                   require a reboot! */
                                if ((strstr(data, "boot") != NULL) ||
                                    (strstr(data, "kernel") != NULL) ||
                                    (strstr(data, "root") != NULL)) {
                                    /* Wait for 10 seconds then reboot */
                                    sleep(10);
                                    system("hub_restart now");
                                } else {
                                    /* Wait for 3 seconds then go back to
				       last led Mode */
                                    sleep(3);
                                    snprintf(cmd, sizeof(cmd) - 1, "%s %s",
                                             LED_CTRL_APP, last_led_mode);
                                    system(cmd);
                                }
                            } else {
                                /* Update last mode */
                                strncpy(last_led_mode, data,
					sizeof(last_led_mode));
                            }
                        }
                    }
                    i += (sizeof(struct inotify_event)) + event->len;
                }
            }
        }
    }
    return NULL;
}

#ifndef beaglebone

// Provisioning support for V3 hub
#ifdef imxdimagic

static void updateWifiStatus(WifiStatusType status)
{
    FILE *f;

    lastWifiStatus = status;
    f = fopen(WIFI_STATUS_FILE, "w");
    if (f != NULL) {
        fprintf(f, "%s", wifiStatusStrs[status]);
        fclose(f);
    }

    // Update LEDs and sound/audio for some modes
    switch (status) {

    case CONNECTING:
        setLedMode("wifi-in-progress");
        // There is no sound for this state...
        break;

    case CONNECTED:
        setLedMode("internet-success");

        // Play sound
        IRIS_playAudio(INET_SUCCESS_VOICE, 0);
        break;

    case NO_INTERNET:
    case NO_SERVER:
        setLedMode("internet-failure");

        // Play sounds
        IRIS_playAudio(FAILURE_SOUND, AUDIO_SHORT_DELAY);
        IRIS_playAudio(INET_FAILURE_VOICE, 0);
        break;

    case BAD_PASS:
    case BAD_SSID:
        setLedMode("wifi-failure");

        // Play sounds
        IRIS_playAudio(FAILURE_SOUND, AUDIO_SHORT_DELAY);
        IRIS_playAudio(WIFI_FAILURE_VOICE, 0);
        break;

    case DISCONNECTED:
    default:
        break;
    }
}

static int checkConnection(void)
{
    char cmd[256];
    FILE *f;
    int count = 0;

    // Check connection via DNS test - Wait at most 60 seconds!
    while (count++ < INTERNET_CHECK_PERIOD) {
        snprintf(cmd, sizeof(cmd), "nslookup %s", TEST_RESOLVE_HOST);
        f = popen(cmd, "r");
        if (f) {
            char line[256];
            char server[128], host[128];
            int  addr_count;

            // Should get a response like:
            //
            // Server:    192.168.1.253
            // Address 1: 192.168.1.253
            //
            // Name:      google.com
            // Address: 172.217.10.14
            //
            // Want to make sure the server address and host address are
            //  not the same as that would signal DNS redirection (likely
            //  because the WAN is down on the router!)
            addr_count = 0;
            server[0] = '\0';
            host[0] = '\0';
            while (1) {
                if (fgets(line, sizeof(line), f) == NULL) {
                    break;
                }

                // Parse off address data
                if (strncmp(line, "Address:", 8) == 0) {
                    char *end, *ptr = line + 11;

                    // Just want address, remove any resolved name
                    end = strchr(ptr, ' ');
                    if (end != NULL) {
                        *end = '\0';
                    }

                    // First address is the server, second the host
                    if (addr_count == 0) {
                        strncpy(server, ptr, sizeof(server));
                    } else {
                        strncpy(host, ptr, sizeof(host));
                    }
                    if (++addr_count == 2) {
                        // If server == host, some sort of DNS redirect occurred
                        if (strncmp(server, host, sizeof(server)) == 0) {
                            break;
                        } else {
                            // Found a result!
                            pclose(f);
                            return 1;
                        }
                    }
                }
            }
            pclose(f);
        }
        sleep(1);
    }
    return 0;
}

static void setProvisioned(char *which)
{
    char cmd[128];

    snprintf(cmd, sizeof(cmd), "echo %s > %s", which, PROVISIONED_FILE);
    system(cmd);
}

// No longer used, but keep around in case we need later...
int isProvisioned(char *which)
{
    char cmd[128];
    FILE *f;

    snprintf(cmd, sizeof(cmd), "cat %s", PROVISIONED_FILE);
    f = popen(cmd, "r");
    if (f) {
        char line[256];
        if (fgets(line, sizeof(line), f) != NULL) {
            if (strncmp(line, which, strlen(which)) == 0) {
                pclose(f);
                return 1;
            }
        }
        pclose(f);
    }
    return 0;
}


static int ethernetProvisioned(void)
{
    syslog(LOG_INFO, "Hub has been provisioned to use Ethernet...");

    // Test connection
    if (checkConnection()) {
        setLedMode("internet-success");

        // Play sound
        IRIS_playAudio(INET_SUCCESS_VOICE, 0);

        // Mark as provisioned on success
        setProvisioned("Ethernet");
        return 1;
    } else {
        setLedMode("internet-failure");

        // Play sounds
        IRIS_playAudio(FAILURE_SOUND, AUDIO_SHORT_DELAY);
        IRIS_playAudio(INET_FAILURE_VOICE, 0);
    }
    return 0;
}

// BLE provisioning handler thread
static void *provisioningThreadHandler(void *ptr)
{
    int done = 0, status_checks = 0;
    int atInit = (int)ptr;
    char cmd[128];
    char line[256];
    time_t last_scan;
    FILE *f;
    time_t restartTime = 0;

    // If we are running at hub init, do a few checks first
    if (atInit) {
        // Sleep a bit to start to wait for networks to come up
        sleep(PROV_INIT_DELAY);

        // If already provisioned, may need to bring up wifi if Ethernet not up
        if (access(PROVISIONED_FILE, F_OK) != -1) {
            if (!IRIS_isIntfConnected(ETH_IF)) {

                // Bring up Wifi if there is config, even if provisioned for Eth
                f = fopen(WIFI_CFG_FILE, "r");
                if (f != NULL) {
                    char ssid[36];

                    fscanf(f, "ssid: %s", ssid);
                    fclose(f);
                    if (ssid[0] != '\0') {
                        syslog(LOG_INFO, "Starting WiFi - SSID: %s", ssid);
                        snprintf(cmd, sizeof(cmd), WIFI_RESTART_CMD, WIFI_IF);
                        system(cmd);
                    }
                }
            }

            // Run the provisioning daemon just to get the BLE firmware version
            snprintf(cmd, sizeof(cmd), "%s -v", WIFI_PROV_DAEMON);
            system(cmd);
            bleProv_thread = 0;
            return NULL;
        }

        // Say this is the first boot
        setLedMode("first-bootup");
        IRIS_playAudio(FIRST_BOOTUP_VOICE, AUDIO_SHORT_DELAY);

        // Check if ethernet cable is plugged in, if so we are done
        if (IRIS_isIntfConnected(ETH_IF)) {
            if (ethernetProvisioned()) {
                // Run the provisioning daemon just to get the BLE firmware version
                snprintf(cmd, sizeof(cmd), "%s -v", WIFI_PROV_DAEMON);
                system(cmd);
                bleProv_thread = 0;
                return NULL;
            }
        }

        // Make sure bluetooth daemon is running
        while (!done) {
            snprintf(cmd, sizeof(cmd), "ps | grep bluetoothd");
            f = popen(cmd, "r");
            if (f) {
                while (fgets(line, sizeof(line), f) != NULL) {
                    if (strstr(line, "grep") == NULL) {
                        done = 1;
                        break;
                    }
                }
                pclose(f);
            }
            sleep(1);
        }
        done = 0;
    } else {

        // Make sure we aren't still trying to get an address!
        system(UDHCPC_WIFI_TERMINATE);

        // We are restarting BLE provisioning due to wifi failure - kill
        //  agent so it doesn't get in the way
        unlink(PROVISIONED_FILE);
        system("killall java");
        restartTime = time(NULL);
        sleep(2);

        // We can re-use the first-bootup LED display, but for 5 minutes
        setLedMode("first-bootup 300");
    }

    // Scan wifi network
    snprintf(cmd, sizeof(cmd), "%s > %s", WIFI_SCAN_CMD, WIFI_SCAN_FILE);
    system(cmd);
    last_scan = time(NULL);

    // Start BLE provisioning daemon
 start_prov:
    snprintf(cmd, sizeof(cmd), "%s&", WIFI_PROV_DAEMON);
    if (system(cmd)) {
        syslog(LOG_ERR, "Unable to start BLE provisioning!");
        bleProv_thread = 0;
        return NULL;
    }

    // Set initial wifi status
    updateWifiStatus(DISCONNECTED);

    // Check status
    while (!done && !(!atInit &&
                      (time(NULL) > (restartTime + BLE_RESTART_PERIOD)))) {

        // Delay before another check
        sleep(WIFI_STATUS_DELAY);

        // If we are restarting BLE provisioning, need to poke watchdog
        //  in case it was started by agent
        if (!atInit) {
            pokeWatchdog();
        }

        // If Ethernet was connected, exit
        if (IRIS_isIntfConnected(ETH_IF)) {
            if (ethernetProvisioned()) {
                break;
            }
        }

        // Rescan wifi network periodically
        if ((last_scan + WIFI_SCAN_PERIOD) < time(NULL)) {
            snprintf(cmd, sizeof(cmd), "%s > %s", WIFI_SCAN_CMD,
                     WIFI_SCAN_FILE);
            system(cmd);
            last_scan = time(NULL);
        }

        // Run state machine
        switch (lastWifiStatus) {
        case DISCONNECTED:
        case BAD_PASS:
        case BAD_SSID:
        case NO_INTERNET:
        case NO_SERVER:
        default:
            // Waiting for config, or fixed config
            if (status_checks) {
                status_checks = 0;

                // Make sure we aren't still trying to get an address!
                system(UDHCPC_WIFI_TERMINATE);

                // Restart wifi provisioning in bad credential cases
                if ((lastWifiStatus == BAD_PASS) ||
                    (lastWifiStatus == BAD_SSID)) {
                    snprintf(cmd, sizeof(cmd), "killall %s", WIFI_PROV_DAEMON);
                    system(cmd);
                }
            }
            break;

        case CONNECTING:
            // Did we get an IP address?
            if (IRIS_isIntfIPUp(WIFI_IF)) {
                status_checks = 0;

                // Set LEDs
                setLedMode("wifi-success");

                // Play audio
                IRIS_playAudio(WIFI_SUCCESS_VOICE, AUDIO_PLAY_DELAY);

                // Set LEDs
                setLedMode("internet-in-progress");

                if (checkConnection()) {
                    updateWifiStatus(CONNECTED);
                } else {
                    updateWifiStatus(NO_INTERNET);
                }
            } else {

                // If SSID is too small or too large, declare error
                //  removed check against scan data as this data is a
                //  moving target and can produce false failures
                if (status_checks == 0) {
                    char ssid[64] = { 0 };

                    snprintf(cmd, sizeof(cmd), "cat %s | grep ssid",
                             WIFI_CFG_FILE);
                    f = popen(cmd, "r");
                    if (f) {
                        if (fgets(line, sizeof(line), f) != NULL) {
                            char *ptr = strstr(line, "ssid: ");
                            if (ptr) {
                                ptr += strlen("ssid: ");
                                strncpy(ssid, ptr, sizeof(ssid));
                                ssid[strlen(ptr) - 1] = '\0';
                            }
                        }
                        pclose(f);
                    }
                    if ((ssid[0] == '\0') || (strlen(ssid) > 32)) {
                        updateWifiStatus(BAD_SSID);
                    }
                } else if (status_checks > MAX_WIFI_STATUS_CHECKS) {
                    // After a while, give up and declare bad password
                    updateWifiStatus(BAD_PASS);
                } else {
                    int result = -1;

                    // Look at status file
                    f = fopen(WIFI_TEST_FILE, "r");
                    if (f != NULL) {
                        fscanf(f, "%d", &result);
                        fclose(f);
                    }

                    // Catch bad password / no address - we'll catch the normal
                    //  case when we check above for the address
                    if (result == 1) {
                        updateWifiStatus(BAD_PASS);
                    } else if (result == 2) {
                        updateWifiStatus(NO_SERVER);
                    }
                }
                status_checks++;
            }
            break;

        case CONNECTED:
            // Done!
            done = 1;

            // Mark as provisioned
            setProvisioned("WiFi");
            break;
        }

        // Make sure provisioning is still running if not done
        if (!done) {
            snprintf(cmd, sizeof(cmd), "ps | grep wifi_prov");
            f = popen(cmd, "r");
            if (f) {
                int found = 0;

                while (fgets(line, sizeof(line), f) != NULL) {
                    if (strstr(line, "grep") == NULL) {
                        found = 1;
                    }
                }
                pclose(f);
                // Restart daemon unless we are factory resetting!
                if (!found && !factory_resetting) {
                    syslog(LOG_ERR, "Restarting BLE provisioning...");
                    goto start_prov;
                }
            }
        }
    }

    // If we exited due to time out, go back to be provisioned via wifi
    //  as before so agent will restart
    if (!done) {
        // Mark as provisioned
        setProvisioned("WiFi");
    }

    syslog(LOG_INFO, "BLE provisioning is complete");

    // Stop BLE provisioning daemon
    snprintf(cmd, sizeof(cmd), "killall %s", WIFI_PROV_DAEMON);
    system(cmd);
    bleProv_thread = 0;
    return NULL;
}
#endif

/* Wifi configuration handler thread */
static void *wifiCfgThreadHandler(void *ptr)
{
    int  fd;
    char buf[512];
    int  len;

    // Starting too early can cause problems with getting notifications
    sleep(WIFI_CFG_INIT_DELAY);

    /* We want to be notified when the wifi config file is changed */
    fd = inotify_init();
    if (fd >= 0) {

        /* Add wifi config file to watch list */
        inotify_add_watch(fd, TEST_WIFI_CFG_FILE, IN_CLOSE_WRITE);

        /* Keep looking for file modifications... */
        while (1) {

            /* Read inotify fd to get events */
            len = read(fd, buf, sizeof(buf));
            if (len >= 0) {
                int i;

                /* Parse events, looking for modify events ... */
                i = 0;

                while (i < len) {
                    struct inotify_event *event =
                      (struct inotify_event *)&buf[i];

                    /* Got a file modification event */
                    if (event->mask & IN_CLOSE_WRITE) {
                        char       cmd[128];
                        int        wifiWasUp = 0, connectStatus;

                        // Note if wifi is currently up
                        if (IRIS_isIntfConnected(WIFI_IF) &&
                            !IRIS_isIntfConnected(ETH_IF)) {
                            wifiWasUp = 1;
                        }

                        // Clear status to start
                        snprintf(cmd, sizeof(cmd), "echo \" \" > %s",
				 WIFI_TEST_FILE);
                        system(cmd);

                        // Test connection first
                        snprintf(cmd, sizeof(cmd), WIFI_TEST_CMD, WIFI_IF,
                                 TEST_WIFI_CFG_FILE);
                        connectStatus = system(cmd);

                        // Update status
                        snprintf(cmd, sizeof(cmd), "echo %d > %s",
                                 WEXITSTATUS(connectStatus), WIFI_TEST_FILE);
                        system(cmd);

                        if (connectStatus) {
                            /* Restart wireless interface if it was up and
                               Ethernet has not come up while testing */
                            if (wifiWasUp && !IRIS_isIntfConnected(ETH_IF)) {
                                snprintf(cmd, sizeof(cmd), WIFI_RESTART_CMD,
                                         WIFI_IF);
                                system(cmd);
                            }
#ifdef imxdimagic
                            // We have tried to connect - have provisioning
                            //  catch errors
                            if (IRIS_isReleaseImage() &&
                                (access(PROVISIONED_FILE, F_OK) == -1)) {
                                updateWifiStatus(CONNECTING);
                            }
#endif
                            i = len;
                            break;
                        }

                        // Use new configuration
                        snprintf(cmd, sizeof(cmd), "cp %s %s",
                                 TEST_WIFI_CFG_FILE, WIFI_CFG_FILE);
                        system(cmd);
                        snprintf(cmd, sizeof(cmd), WIFI_RESTART_CMD, WIFI_IF);
#ifdef imxdimagic
                        // We can now connect...
                        if (IRIS_isReleaseImage() &&
                            (access(PROVISIONED_FILE, F_OK) == -1)) {
                            updateWifiStatus(CONNECTING);
                            system(cmd);
                        } else
#endif
                        /* Restart wireless interface if it was up and
                           Ethernet has not come up while testing */
                        if (wifiWasUp && !IRIS_isIntfConnected(ETH_IF)) {
                            system(cmd);
                        }

                        // Ignore multiple notifications
                        i = len;
                        break;
                    }
                    i += (sizeof(struct inotify_event)) + event->len;
                }
            }
        }
    } else {
        syslog(LOG_ERR, "Error getting notify fd!!!");
    }
    return NULL;
}

#endif

#ifdef imxdimagic
static void checkForRadioFirmwareUpdates()
{
    char cmd[256], filepath[256], origpath[256];

    snprintf(origpath, sizeof(origpath), "%s%s", ROOT_FW_DIR,
             ZIGBEE_FIRMWARE);

    // Is there zigbee agent firmware in the build?
    if (access(origpath, F_OK) != -1) {
        snprintf(filepath, sizeof(filepath), "%s%s", FIRMWARE_DIR,
                 ZIGBEE_FIRMWARE);

        // A new binary?
        snprintf(cmd, sizeof(cmd), "diff -q %s %s", origpath, filepath);
        if (system(cmd)) {
            unlink(filepath);
            syslog(LOG_INFO, "Installing Zigbee firmware...");
            sleep(1); // Wait for init process to finish
            snprintf(cmd, sizeof(cmd), "zigbee_flash -w %s", origpath);
            if (system(cmd)) {
                syslog(LOG_ERR, "Error installing Zigbee firmware!\n");
                // Touch file so that agent starts up - we will
                //  try to install again on next boot
                snprintf(cmd, sizeof(cmd), "touch %s", filepath);
                system(cmd);
                return;
            }
            // Copy file to final location only upon success
            IRIS_copyFile(origpath, filepath);
            chmod(filepath, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
            chown(filepath, 0, 0);
        }
    }
}
#endif

/* Simple init task - just check for button pushes for now */
int main(int argc, char** argv)
{
    GError     *err = 0;
    GMainLoop  *loop = g_main_loop_new(0, 0);
    gsize      read_count = 0;
    gchar      buf[64];
    int        fdGpio, restart = 0;
    pthread_t  led_thread;
#ifndef beaglebone
    pthread_t  wifiCfg_thread;
#endif
    FILE       *f;

    /* Create version file */
    f = fopen(VERSION_FILE, "w");
    if (f != NULL) {
        fprintf(f, "v%d.%d.%d.%03d%s\n", VERSION_MAJOR, VERSION_MINOR,
		VERSION_POINT, VERSION_BUILD, VERSION_SUFFIX);
        fclose(f);
    }

    /* Are we being restarted? */
    if ((argv[1] != NULL) &&
        (strncmp(argv[1], "restart", strlen("restart")) == 0)) {
        restart = 1;
    }

    /* Create empty led Mode file */
    setLedMode("all-off");

    /* Turn ourselves into a daemon */
    daemon(1, 1);

    /* Setup syslog */
    openlog(SYSLOG_IDENT, (LOG_CONS | LOG_PID | LOG_PERROR), LOG_DAEMON);

    /* Create semaphore to wait for inotify code to be ready */
    sem_init(&led_sem, 0, 0);

    /* Create LED handler thread - may need to sleep for some actions */
    pthread_create(&led_thread, NULL, &ledThreadHandler, NULL);

#ifndef beaglebone
    /* Create wifi configuration handler */
    pthread_create(&wifiCfg_thread, NULL, &wifiCfgThreadHandler, NULL);
#endif

#ifdef imxdimagic
    /* See if Zigbee firmware needs to be upgraded and create
       BLE provisioning handler */
    if (IRIS_isReleaseImage()) {
        int atInit = 1;

        /* Is there radio firmware to update?  Needed for v3 hub to work
           around a Zigbee install issue in the mfg firmware. */
        checkForRadioFirmwareUpdates();

        pthread_create(&bleProv_thread, NULL, &provisioningThreadHandler,
                       (void *)atInit);
    }
#endif

    /* Skip reset button support for manufacturing image as it will
       get in the way of diagnostic testing */
    if (!IRIS_isMfgImage()) {

        /* Open GPIO - reset button */
        fdGpio = open(BUTTON_GPIO_VALUE_FILE, O_RDONLY | O_NONBLOCK);

        /* Create IO channel */
        if (fdGpio != -1) {
            buttonChannel = g_io_channel_unix_new(fdGpio);

            if (buttonChannel) {
                /* Read channel to clear initial button state */
                g_io_channel_read_chars(buttonChannel, buf, sizeof(buf) - 1,
                                        &read_count, &err);

                /* Watch for changes to this IO channel */
                g_io_add_watch(buttonChannel, G_IO_PRI, buttonHandler,
			       (gpointer)0);
            }
        }

#ifdef imxdimagic
        /* Open GPIO - IRIS button */
        fdGpio = open(IRIS_BUTTON_GPIO_VALUE_FILE, O_RDONLY | O_NONBLOCK);

        /* Create IO channel */
        if (fdGpio != -1) {
            GIOChannel *channel = g_io_channel_unix_new(fdGpio);

            if (channel) {
                /* Read channel to clear initial button state */
                g_io_channel_read_chars(channel, buf, sizeof(buf) - 1,
                                        &read_count, &err);

                /* Watch for changes to this IO channel */
                g_io_add_watch(channel, G_IO_PRI, irisButtonHandler,
			       (gpointer)0);
            }
        }

        /* Open GPIO - Power down button */
        fdGpio = open(PWR_DOWN_BTN_GPIO_VALUE_FILE, O_RDONLY | O_NONBLOCK);

        /* Create IO channel */
        if (fdGpio != -1) {
            GIOChannel *channel = g_io_channel_unix_new(fdGpio);

            if (channel) {
                /* Read channel to clear initial button state */
                g_io_channel_read_chars(channel, buf, sizeof(buf) - 1,
                                        &read_count, &err);

                /* Watch for changes to this IO channel */
                g_io_add_watch(channel, G_IO_PRI, pwrDownButtonHandler,
			       (gpointer)0);
            }
        }
#endif
    }

#ifdef beaglebone
    /* Open GPIO - USB0 Over-current */
    fdGpio = open(USB0_OC_GPIO_VALUE_FILE, O_RDONLY | O_NONBLOCK);

    /* Create IO channel */
    if (fdGpio != -1) {
        GIOChannel *channel = g_io_channel_unix_new(fdGpio);

        if (channel) {
            /* Read channel to clear initial button state */
            g_io_channel_read_chars(channel, buf, sizeof(buf) - 1,
                                    &read_count, &err);

            /* Watch for changes to this IO channel */
            g_io_add_watch(channel, G_IO_PRI, usbOCHandler, (gpointer)0);
        }
    }

    /* Open GPIO - USB1 Over-current */
    fdGpio = open(USB1_OC_GPIO_VALUE_FILE, O_RDONLY | O_NONBLOCK);

    /* Create IO channel */
    if (fdGpio != -1) {
        GIOChannel *channel = g_io_channel_unix_new(fdGpio);

        if (channel) {
            /* Read channel to clear initial button state */
            g_io_channel_read_chars(channel, buf, sizeof(buf) - 1,
                                    &read_count, &err);

            /* Watch for changes to this IO channel */
            g_io_add_watch(channel, G_IO_PRI, usbOCHandler, (gpointer)1);
        }
    }
#endif

    /* Wait or inotify support to be ready */
    sem_wait(&led_sem);

    /* For manufacturing, start with all LEDs off to avoid confusion
       during the test process (at least for v2 hub...) */
    if (IRIS_isMfgImage()) {
#ifdef beaglebone
        setLedMode("all-off");
#endif
    } else {
        /* Pause for a moment to make sure initial pattern is displayed -
           avoid odd flashing patterns */
        sleep(1);

        /* If not restart, set initial LEDs */
        if (!restart) {
            setLedMode("boot-linux");
        }
    }

    /* Add in timeout routine to check if SSH access should be allowed
        This is only for release image! */
    if (IRIS_isReleaseImage()) {
        g_timeout_add_seconds(SSH_PERIODIC, sshCheckAccess, NULL);
    } else {
        char  cmd[128];
        // For other images, always start up SSH (after waiting to make
        //  sure keys have been generated)
        do {
            sleep(1);
        } while (IRIS_isSSHKeyGen());
        snprintf(cmd, sizeof(cmd), "%s realstart", SSH_INIT_SCRIPT);
        system(cmd);

        /* Lower console print level to avoid watchdog logs! */
        system("sysctl -w kernel.printk=\"1 4 1 7\" > /dev/null 2>&1");
    }

    /* Make sure ifplugd is always running */
    g_timeout_add_seconds(IFPLUGD_PERIODIC, ifplugdWatchdog, NULL);

    /* For non-release image, need to poke the watchdog - done by the agent
       in release image */
    if (!IRIS_isReleaseImage()) {
        int timeout = WATCHDOG_PERIOD;
        int fd;

        /* Make sure watchdog period is correct (5 minutes, rather
           than default of 1) */
        fd = open(HW_WATCHDOG_DEV, O_RDWR);
        if (fd != -1) {
            ioctl(fd, WDIOC_SETTIMEOUT, &timeout);
            close(fd);

            /* Set up periodic to poke watchdog */
            g_timeout_add_seconds(WATCHDOG_POKE_PERIODIC, hwWatchdog, NULL);
        }
    }

    /* Enable alarm handler */
    signal(SIGALRM, alarmHandler);

    /* Set up sound support */
#if defined(imxdimagic)
    system("amixer cset numid=37 63 > /dev/null 2>&1");
    system("amixer cset numid=7 255 > /dev/null 2>&1");
#endif

    /* Handle input */
    g_main_loop_run(loop);
    return 0;
}


