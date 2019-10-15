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

#define _XOPEN_SOURCE 500
#define _DEFAULT_SOURCE
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <ftw.h>
#include <pwd.h>
#include <irislib.h>

#define INITIAL_DELAY      4
#define SYSLOG_IDENT       "irisagentd"

#define AGENT_START_SCRIPT "/data/agent/bin/iris-agent"
#define AGENT_LOG_FILE     "/tmp/hubAgent.log"
#define AGENT_LAST_LOG     "/tmp/hubAgent.last"
#define COLLECTD_START     "/etc/init.d/collectd start"
#define AGENT_DEF_TARFILE  "/home/agent/iris-agent-hub"
#define UNPACK200_BINARY   "/usr/lib/jvm/java-7-openjdk/jre/bin/unpack200"
#define STARTUP_SOUND      "/data/agent/conf/sounds/Start_Up.mp3"
#define ZIGBEE_FW_FILE     "/data/firmware/zigbee-firmware-hwflow.bin"

/* Agent debug configuration related defines */
#define HOSTS_FILE         "/var/run/hosts"
#define DEFAULT_HOSTNAME   "bh.irisbylowes.com"
#define HUB_OS_BIN         "hubOS.bin"
#define HUB_AGENT_BIN      "hubAgent.bin"
#define HUB_AGENT_CKSUM    "/data/agent/lastAgentCksum.txt"

/* Agent watchdog configuration */
#define SOFT_RESET_THRESHOLD   3
#define FACTORY_DEF_THRESHOLD  6
#define MIN_RUNNING_PERIOD     (30 * 60)

/* NTP time check on boot */
#define MAX_NTP_WAIT           300
#define VALID_NTP_TIME         (60 * 60 * 24 * 365 * (2015 - 1970))


/* Check for debug firmware */
void checkForHubUpdates(void)
{

    /* Is SSH enabled?  If so, look to see if new hubOS/hubAgent code exists
       on the dongle */
    if (IRIS_isSSHEnabled()) {
        char cmd[512];
        int  res;
        FILE *f;
        char ledMode[64] = { 0 };
        char filename[128];

        /* Look in media mount location to see if we can find debug files */
        chdir(MEDIA_MOUNT_DIR);

        /* Get current LED mode so we can restore later */
        IRIS_getLedMode(ledMode, sizeof(ledMode));

        snprintf(cmd, sizeof(cmd),
                 "find . -name %s -maxdepth %d | grep -v \'/\\.\'",
                 HUB_OS_BIN, MAX_FIND_DEPTH);
        f = popen(cmd, "r");
        if (f) {
            if (fgets(filename, sizeof(filename), f) != NULL) {
                /* Removing trailing \n */
                if (filename[strlen(filename)-1] == '\n') {
                    filename[strlen(filename)-1] = '\0';
                }

                syslog(LOG_INFO, "Found debug firmware on dongle (%s)...",
                       filename);

                syslog(LOG_INFO, "Validating firmware...");
                snprintf(cmd, sizeof(cmd), "validate_image %s", filename);
                res = system(cmd);

                /* If validation passes, install */
                if (!res && (access(FW_INSTALL_FILE, F_OK) != -1)) {
                    syslog(LOG_INFO, "Installing firmware...");
                    snprintf(cmd, sizeof(cmd), "fwinstall %s", FW_INSTALL_FILE);
                    res = system(cmd);

                    /* Reboot if install was successful */
                    if (!res) {
                        syslog(LOG_INFO,
                           "Debug firmware install was successful - rebooting");
                        pclose(f);
                        system("hub_restart 2");
                        exit(0);
                    } else {
                        syslog(LOG_INFO,
                           "Error installing debug firmware - using existing.");
                    }
                } else {
                    syslog(LOG_INFO,
                           "Error validating debug firmware - using existing.");
                }
            }
        }
        pclose(f);

        /* Check for device key */
        f = fopen(MAC_ADDR1_FILE, "r");
        if (f != NULL) {
            char macAddr[24];

            if (fgets(macAddr, sizeof(macAddr), f) != NULL) {
                /* Remove trailing \n */
                if (macAddr[strlen(macAddr)-1] == '\n') {
                    macAddr[strlen(macAddr)-1] = '\0';
                }
                fclose(f);

                /* Do we already have a key? */
                snprintf(cmd, sizeof(cmd), "/tmp/mfg/keys/%s.key", macAddr);
                if (access(cmd, F_OK) == -1) {
                    int i;

                    /* See if file in on dongle, name has '_' rather than ':' */
                    for (i = 0; i < strlen(macAddr); i++) {
                        if (macAddr[i] == ':') {
                            macAddr[i] = '_';
                        }
                    }
                    snprintf(cmd, sizeof(cmd),
                          "find . -name %s.key -maxdepth %d | grep -v \'/\\.\'",
                             macAddr, MAX_FIND_DEPTH);
                    f = popen(cmd, "r");
                    if (f) {
                        if (fgets(filename, sizeof(filename), f) != NULL) {
                            /* Removing trailing \n */
                            if (filename[strlen(filename)-1] == '\n') {
                                filename[strlen(filename)-1] = '\0';
                            }
                            syslog(LOG_INFO, "Found hub key on dongle (%s)...",
                                   filename);
                            snprintf(cmd, sizeof(cmd), "update_key %s",
                                     filename);
                            if (system(cmd)) {
                                syslog(LOG_INFO, "Error installing hub key!");
                            } else {
                                syslog(LOG_INFO, "Hub key has been installed - restarting...");
                                pclose(f);
                                system("hub_restart 2");
                                exit(0);
                            }
                        }
                        pclose(f);
                    }
                }
            } else {
                fclose(f);
            }
        }

        /* Check for a Hub agent update */
        snprintf(cmd, sizeof(cmd),
                 "find . -name %s -maxdepth %d | grep -v \'/\\.\'",
                 HUB_AGENT_BIN, MAX_FIND_DEPTH);
        f = popen(cmd, "r");
        if (f) {
            int install = 0;

            if (fgets(filename, sizeof(filename), f) != NULL) {
                /* Removing trailing \n */
                if (filename[strlen(filename)-1] == '\n') {
                    filename[strlen(filename)-1] = '\0';
                }

                syslog(LOG_INFO, "Found debug hub agent on dongle (%s)...",
                       filename);

                /* Have we installed an agent before? */
                if (access(HUB_AGENT_CKSUM, F_OK) != -1) {
                    /* Check checksum */
                    snprintf(cmd, sizeof(cmd), "sha256sum -s -c %s",
                             HUB_AGENT_CKSUM);
                    /* Install if checksum is not correct */
                    if (system(cmd)) {
                        install = 1;
                    }
                } else {
                    install = 1;
                }

                if (install) {
                    /* Save checksum */
                    snprintf(cmd, sizeof(cmd), "sha256sum %s > %s", filename,
                             HUB_AGENT_CKSUM);
                    system(cmd);

                    /* Reinstall agent code and set permissions correctly */
                    snprintf(cmd, sizeof(cmd),
                             "rm -rf /data/iris; rm -rf /data/agent/libs;"
                             " cd /data/agent; chown -R root .;"
                             " chgrp -R root .; tar xf %s%s;"
                             " chown -R agent .; chgrp -R agent .",
                             MEDIA_MOUNT_DIR, &filename[1]);
                    if (system(cmd)) {
                        unlink(HUB_AGENT_CKSUM);
                        syslog(LOG_ERR, "Error updating Hub agent code!");
                    } else {
                        syslog(LOG_INFO, "Hub agent code has been updated.");
                    }
                } else {
                    syslog(LOG_INFO,
                       "Latest debug Hub agent code is already installed.");
                }
            }
        }
        pclose(f);

        /* Restore LEDs */
        IRIS_setLedMode(ledMode);
    }
}


/* Add a host entry to map platform server */
static int createHostEntry(char *hostip, char *hostname)
{
    FILE *f;
    char *host = DEFAULT_HOSTNAME;

    f = fopen(HOSTS_FILE, "w");
    if (f == NULL) {
        return -1;
    }

    /* Add in local host info */
    fprintf(f, "127.0.0.1\tlocalhost.localdomain\tlocalhost\n");

    /* Has platform host has been given? */
    if (hostname[0]) {
        host = hostname;
    }
    fprintf(f, "%s\t%s\n", hostip, host);
    fclose(f);
    return 0;
}


/* Restore default host file */
static void clearHostEntries(void)
{
    FILE *f;

    f = fopen(HOSTS_FILE, "w");
    if (f == NULL) {
        return;
    }

    /* Add in local host info only */
    fprintf(f, "127.0.0.1\tlocalhost.localdomain\tlocalhost\n");
    fclose(f);
    return;
}


/* Parse agent debug configuration file */
static int parseDebugConfig(char *filename)
{
    FILE *f;
    char line[256];
    char flags[256] = { 0 };
    char var[128] = { 0 };
    char value[256] = { 0 };
    char platformIP[20] = { 0 };
    char platformHost[64] = { 0 };
    int  varSet = 0;

    f = fopen(filename, "r");
    if (f == NULL) {
        syslog(LOG_ERR, "Unable to open %s to get debug configuration!",
               filename);
        return -1;
    }

    /* Parse debug configuration */
    while (fgets(line, sizeof(line), f) != NULL) {
        if (strstr(line, "platform_ip")) {
            if (sscanf(line, "platform_ip = %16s", platformIP) > 0) {
                syslog(LOG_DEBUG, "Platform IP: %s", platformIP);
            }
        } else if (strstr(line, "platform_host")) {
            if (sscanf(line, "platform_host = %63s", platformHost) > 0) {
                syslog(LOG_DEBUG, "Platform Host: %s", platformHost);
            }
        } else if (strstr(line, "agent_debug")) {
            if (sscanf(line, "agent_debug = %[^\n]", flags) > 0) {
                syslog(LOG_DEBUG, "Agent Debug: %s", flags);
            }
            /* Parse environment variable settings, skip lines that
               start with a comment '#' */
        } else if (strstr(line, " = ") && (line[0] != '#')) {
            /* Allow the user to set any environment variable data ... */
            if (sscanf(line, "%s = %[^\n]", var, value) > 0) {
                syslog(LOG_DEBUG, "Var: %s = %s", var, value);

                /* Clear variable if it has been set to "" */
                if ((value[0] == '"') && (value[1] == '"')) {
                    unsetenv(var);
                } else {
                    setenv(var, value, 1);
                }
                varSet = 1;
            }
        }
    }

    /* Add in host entry if platform IP address has been configured and
        additional variables were not provided */
    if ((platformIP[0] != '\0') && !varSet) {
        if (createHostEntry(platformIP, platformHost)) {
            fclose(f);
            return -1;
        }
    } else {
        clearHostEntries();
    }

    /* Set agent debug flags environment variable if available */
    if (flags[0] != '\0') {
        setenv("JAVA_DBG_OPTS", flags, 1);
    } else {
        unsetenv("JAVA_DBG_OPTS");
    }
    fclose(f);
    return 0;
}


/* Fire up hub agent */
static void startHubAgent(void)
{
    char cmd[256];
    pid_t child;
    FILE *f;
    char hubID[32];
    char cfgFile[64] = { 0 };

#ifdef imxdimagic
    // If we are not yet provisioned, don't start agent as it will
    //  reset the hub due to a lack of connection if provisioning doesn't
    //  finish quickly!
    while (access(PROVISIONED_FILE, F_OK) == -1) {
        sleep(1);
    }

    // Also, make sure we are not installing radio firmware
    while (access(ZIGBEE_FW_FILE, F_OK) == -1) {
        sleep(1);
    }
#endif

    /* Look in media mount location to see if we can find a key file */
    chdir(MEDIA_MOUNT_DIR);

    snprintf(cmd, sizeof(cmd), "cat %s", HUB_ID_FILE);
    f = popen(cmd, "r");
    if (f) {
        fscanf(f, "%s", hubID);
    }
    pclose(f);

    /* Config file will be named hubID.cfg - e.g LWC-2542.cfg */
    if (hubID[0] != '\0') {
        /* Removing trailing \n */
        if (hubID[strlen(hubID)-1] == '\n') {
            hubID[strlen(hubID)-1] = '\0';
        }
        snprintf(cfgFile, sizeof(cfgFile), "%s.cfg", hubID);
    }

    /* Look for agent debug configuration */
    if (cfgFile[0] != '\0') {
        char filename[128];

        snprintf(cmd, sizeof(cmd),
                 "find . -name %s -maxdepth %d | grep -v \'/\\.\'",
                 cfgFile, MAX_FIND_DEPTH);
        f = popen(cmd, "r");
        if (f) {
            if (fgets(filename, sizeof(filename), f) != NULL) {
                /* Removing trailing \n */
                if (filename[strlen(filename)-1] == '\n') {
                    filename[strlen(filename)-1] = '\0';
                }

                /* Parse configuration file */
                if (parseDebugConfig(filename)) {
                    syslog(LOG_ERR, "Error parsing agent debug config!");
                }
                pclose(f);
            } else {
                pclose(f);
                goto no_cfg;
            }
        } else {
            goto no_cfg;
        }
    } else {
        /* Clear debug info */
    no_cfg:
        syslog(LOG_DEBUG, "No agent debug configuration is present.");
        unsetenv("JAVA_DBG_OPTS");
        clearHostEntries();
    }

    syslog(LOG_INFO, "Starting hub agent...");
    child = fork();
    if (child >= 0) {
        if (child == 0) {

            // Set leds to agent boot
            IRIS_setLedMode("boot-agent");

#ifdef imxdimagic
            // Play start up sound
            IRIS_playAudio(STARTUP_SOUND, 0);
#endif

            // Run the hub agent as user "agent"
            //  Note: must append to log file (via >>) for logrotate
            // to work correctly!
            snprintf(cmd, sizeof(cmd), "su - agent -p -c \"%s >> %s 2>&1\"",
                     AGENT_START_SCRIPT, AGENT_LOG_FILE);
            system(cmd);
            exit(0);
        } else {
            int status;

            // Wait for shell to terminate to avoid a zombie process
            wait(&status);
        }
    } else {
        syslog(LOG_ERR, "Error starting Hub agent!");
    }
}

/* File remove callback */
static int file_remove_cb(const char *fpath, const struct stat *sb,
                          int typeflag, struct FTW *ftwbuf)
{
    return remove(fpath);
}

/* File chown callback */
static int file_chown_cb(const char *fpath, const struct stat *sb,
                          int typeflag, struct FTW *ftwbuf)
{
    struct passwd *pwd = getpwnam("agent");
    if (pwd == NULL) {
        return -1;
    }
    return chown(fpath, pwd->pw_uid, pwd->pw_gid);
}

/* File unpack200 callback */
static int file_unpack200_cb(const char *fpath, const struct stat *sb,
                          int typeflag, struct FTW *ftwbuf)
{
    struct passwd *pwd = getpwnam("agent");
    char filename[256], cmd[512], *p;

    strncpy(filename, fpath, sizeof(filename));

    /* Does file end with .pack.gz? */
    if ((p = strstr(filename, ".pack.gz")) == NULL) {
        return 0;
    }
    *p = '\0';

    /* Remove old jar file */
    snprintf(cmd, sizeof(cmd), "%s.jar", filename);
    unlink(cmd);

    /* Use unpack200 to unpack file */
    snprintf(cmd, sizeof(cmd), "%s %s %s.jar",
             UNPACK200_BINARY, fpath, filename);
    if (system(cmd)) {
        syslog(LOG_ERR, "Error unpacking %s.pack.gz\n", filename);
        return -1;
    }
    /* Remove file only after unpack has completed in case we are rebooted */
    unlink(fpath);
    sync();

    /* Make sure agent can access file */
    snprintf(cmd, sizeof(cmd), "%s.jar", filename);
    return chown(cmd, pwd->pw_uid, pwd->pw_gid);
}

/* Factory reset hub agent by deleting current instance and re-installing */
static void factoryDefaultHubAgent(void)
{
    char cmd[256];
    FILE *f;

    /* Agent is no longer running at this point, just delete data, re-install */
    if (nftw(AGENT_ROOT_DIR, file_remove_cb, 64, FTW_DEPTH | FTW_PHYS) != 0) {
        syslog(LOG_ERR, "Error removing hub agent: %s\n", strerror(errno));
    }
    if (nftw(DATA_IRIS_DIR, file_remove_cb, 64, FTW_DEPTH | FTW_PHYS) != 0) {
        syslog(LOG_ERR, "Error removing hub agent data: %s\n", strerror(errno));
    }
    if (mkdir(AGENT_ROOT_DIR, S_IRWXU | S_IRWXG | S_IRWXO)) {
        syslog(LOG_ERR, "Error creating hub agent directory: %s\n",
               strerror(errno));
        return;
    }
    chdir(AGENT_ROOT_DIR);

    /* Expand default agent tar file */
    snprintf(cmd, sizeof(cmd), "tar xf %s", AGENT_DEF_TARFILE);
    if (system(cmd)) {
        syslog(LOG_ERR, "Error installing hub agent: %s\n", strerror(errno));
        return;
    }

    /* Create factory_reset file to mark that we have started over */
    f = fopen(AGENT_FACTORY_DEFAULT, "w");
    if (f == NULL) {
        syslog(LOG_ERR, "Error creating hub agent factory default file: %s\n",
               strerror(errno));
    }
    fclose(f);

    /* Make sure agent can delete this file */
    chmod(AGENT_FACTORY_DEFAULT,
	  S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);

    /* Change file ownership */
    if (nftw(AGENT_ROOT_DIR, file_chown_cb, 64, FTW_DEPTH | FTW_PHYS) != 0) {
        syslog(LOG_ERR, "Error changing hub agent permissions: %s\n",
               strerror(errno));
    }

    /* Make sure all jar files are unpacked */
    nftw(AGENT_LIBS_DIR, file_unpack200_cb, 64, FTW_DEPTH | FTW_PHYS);

    /* Make sure to sync filesystem! */
    system("sync");

    syslog(LOG_INFO, "Hub Agent has been factory defaulted!");
}

/* Soft reset hub agent by creating indicator file */
static void softResetHubAgent(void)
{
    FILE *f;

    /* Create soft_reset file to mark that we need a reset */
    f = fopen(AGENT_SOFT_RESET, "w");
    if (f == NULL) {
        syslog(LOG_ERR, "Error creating hub agent soft reset file: %s\n",
               strerror(errno));
    }
    fclose(f);

    /* Make sure agent can delete this file */
    chmod(AGENT_SOFT_RESET, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);

    syslog(LOG_INFO, "Signalling Hub Agent to perform soft reset...");
}


// IRIS Hub Agent daemon - start up hub agent and restart if it stops
int main(int argc, char** argv)
{
    int restartCount = 0;
#ifdef COLLECTD_NEEDED
    int i;
#endif
    time_t lastRestart;

    // Turn ourselves into a daemon
    daemon(1, 1);

    // Setup syslog
    openlog(SYSLOG_IDENT, (LOG_CONS | LOG_PID | LOG_PERROR), LOG_DAEMON);

    // Sleep for a bit to let system complete startup
    sleep(INITIAL_DELAY);

    // Does agent exist?  If not, no point is going on...
    if (access(AGENT_START_SCRIPT, F_OK) == -1) {
        syslog(LOG_ERR, "Hub agent does not exist - cannot start!");
        exit(1);
    }

    // Check for any debug firmware/agent updates before we start agent
    checkForHubUpdates();

    // Make sure all jar files are unpacked
    if (nftw(AGENT_LIBS_DIR, file_unpack200_cb, 64, FTW_DEPTH | FTW_PHYS)) {
        // On error, factory default agent
        factoryDefaultHubAgent();
    }

#ifdef COLLECTD_NEEDED
    // Wait a bit for network time - collectd will report noisy logs if time
    //  is not valid.
    for (i = 0; i < MAX_NTP_WAIT; i++) {
        if (time(NULL) > VALID_NTP_TIME) {
            break;
        }
        sleep(1);
    }

    // Start up collectd daemon to gather stats - only needed for agent
    //  builds and we want to start after boot to make sure we have
    //  network time
    if (system(COLLECTD_START)) {
        syslog(LOG_ERR, "Hub agent - error starting statistics collection!");
    }
#endif

    // Start Hub agent
    lastRestart = time(NULL);
    startHubAgent();

    // Hub agent check loop - startHubAgent waits for agent to die!
    while (1) {
        char cmd[128];

        // Start up again after moving log
        if (access(AGENT_LOG_FILE, F_OK) != -1) {
            snprintf(cmd, sizeof(cmd), "mv %s %s", AGENT_LOG_FILE,
                     AGENT_LAST_LOG);
            system(cmd);
        }
        syslog(LOG_ERR, "Hub agent is no longer running - restarting!");

        // Look for constant agent failures
        if ((time(NULL) - lastRestart) < MIN_RUNNING_PERIOD) {
            restartCount++;

            // Should we do a factory default or soft reset?
            if ((FACTORY_DEF_THRESHOLD != 0) &&
                (restartCount > FACTORY_DEF_THRESHOLD) &&
                (access(AGENT_FACTORY_DEFAULT, F_OK) != -1)) {
                factoryDefaultHubAgent();

                // Reset count of restarts on factory reset
                restartCount = 0;
            } else if (restartCount > SOFT_RESET_THRESHOLD) {
                softResetHubAgent();
            }
        } else {
            // Reset count of restarts on long run
            restartCount = 0;
        }

        // Restart agent
        lastRestart = time(NULL);
        startHubAgent();
    }

    /* Should never get here ... */
    exit(0);
}

