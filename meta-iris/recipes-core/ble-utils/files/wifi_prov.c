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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <pthread.h>
#include <time.h>
#include <ctype.h>
#include <sys/signalfd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <glib.h>
#include <dbus/dbus.h>
#include <bluez-dbus.h>
#include <irislib.h>
#include <aes.h>

// Syslog name
#define SYSLOG_IDENT                    "wifi_prov"

// Service UUIDs
#define DEVICE_INFO_SERVICE_UUID        "0000180a-0000-1000-8000-00805f9b34fb"
#define WIFI_CFG_SERVICE_UUID           "9dab269a-0000-4c87-805f-bc42474d3c0b"

// Characteristic UUIDs
#define MODEL_NUMBER_UUID               "00002a24-0000-1000-8000-00805f9b34fb"
#define SERIAL_NUMBER_UUID              "00002a25-0000-1000-8000-00805f9b34fb"
#define FIRMWARE_REV_UUID               "00002a26-0000-1000-8000-00805f9b34fb"
#define HARDWARE_REV_UUID               "00002a27-0000-1000-8000-00805f9b34fb"
#define MFG_NAME_UUID                   "00002a29-0000-1000-8000-00805f9b34fb"

#define WIFI_SCAN_RESULTS_UUID          "9dab269a-0001-4c87-805f-bc42474d3c0b"
#define WIFI_SUP_MODES_UUID             "9dab269a-0002-4c87-805f-bc42474d3c0b"
#define WIFI_SUP_FREQS_UUID             "9dab269a-0003-4c87-805f-bc42474d3c0b"
#define WIFI_CUR_MODE_UUID              "9dab269a-0004-4c87-805f-bc42474d3c0b"
#define WIFI_CUR_FREQ_UUID              "9dab269a-0005-4c87-805f-bc42474d3c0b"
#define WIFI_CFG_SSID_UUID              "9dab269a-0006-4c87-805f-bc42474d3c0b"
#define WIFI_AUTH_TYPE_UUID             "9dab269a-0007-4c87-805f-bc42474d3c0b"
#define WIFI_ENC_TYPE_UUID              "9dab269a-0008-4c87-805f-bc42474d3c0b"
#define WIFI_CFG_PASSWD_UUID            "9dab269a-0009-4c87-805f-bc42474d3c0b"
#define WIFI_STATUS_UUID                "9dab269a-000a-4c87-805f-bc42474d3c0b"

// Static strings
#define MFG_NAME                        "Lowe's Companies, Inc."
#define SUPPORTED_MODES                 "B,G,N"
#define SUPPORTED_FREQS                 "2.4"
#define CURRENT_MODE                    "N"
#define CURRENT_FREQ                    "2.4"
#define DEFAULT_STATUS                  "DISCONNECTED"
#define SSID_KEY                        "{\"ssid\":"

#define SCAN_RESULTS_DESC               "WiFi Scan"
#define SUPPORTED_MODES_DESC            "WiFi Modes"
#define SUPPORTED_FREQS_DESC            "WiFi Freqs."
#define CURRENT_MODE_DESC               "Current Wifi Mode"
#define CURRENT_FREQ_DESC               "Current Wifi Frequency"
#define CONFIG_SSID_DESC                "WiFi Config SSID"
#define AUTH_TYPE_DESC                  "WiFi Authentication Type"
#define ENC_TYPE_DESC                   "WiFi Encryption Type"
#define CONFIG_PASSWD_DESC              "Wifi Config Password"
#define STATUS_DESC                     "WiFi Status"

// Command strings - cryptic data to send commands to device!
#define SET_PORT_UP_CMD         "hciconfig hci0 up"
#define SET_PORT_DOWN_CMD       "hciconfig hci0 down"
#define SET_ALIAS_CMD           "dbus-send --system --print-reply --dest=org.bluez /org/bluez/hci0 org.freedesktop.DBus.Properties.Set string:org.bluez.Adapter1 string:Alias variant:string:%s > /dev/null 2>&1"
#define GET_STATIC_ADDRS_CMD    "hcitool -i hci0 cmd 0x3F 0x0009"
#define GET_LOCAL_VERSION_CMD   "hcitool -i hci0 cmd 0x3F 0x0001"
#define SET_STATIC_ADDR_CMD     "hcitool -i hci0 cmd 0x08 0x0005 %s > /dev/null 2>&1"
#define SET_ADV_DATA_CMD        "hcitool -i hci0 cmd 0x08 0x0008 18 02 01 06 02 0A 00 11 07 0B 3C 4D 47 42 BC 5F 80 87 4C 00 00 9a 26 AB 9D 00 00 00 00 00 00 00 > /dev/null 2>&1"
#define SET_DISCOVERABLE_CMD    "dbus-send --system --print-reply --dest=org.bluez /org/bluez/hci0 org.freedesktop.DBus.Properties.Set string:org.bluez.Adapter1 string:Discoverable variant:boolean:%s > /dev/null 2>&1"
#define SET_DISC_TIMEOUT_CMD    "dbus-send --system --print-reply --dest=org.bluez /org/bluez/hci0 org.freedesktop.DBus.Properties.Set string:org.bluez.Adapter1 string:DiscoverableTimeout variant:uint32:%d > /dev/null 2>&1"
#define SET_ADV_PARAMS_CMD      "hcitool -i hci0 cmd 0x08 0x0006 00 08 00 08 00 01 00 00 00 00 00 00 00 07 00 > /dev/null 2>&1"
#define SET_ADV_ENABLE_CMD      "hcitool -i hci0 cmd 0x08 0x000A 01 > /dev/null 2>&1"
#define SET_ADV_RESP_DATA_CMD   "hcitool -i hci0 cmd 0x08 0x0009 17 16 09 49 72 69 73 5F 48 75 62 5F %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X 00 00 00 00 00 00 00 00 > /dev/null 2>&1"
#define STATIC_ADDR_HDR         "01 09 FC 00 01 "
#define LOCAL_VERSION_HDR       "01 01 FC 00 02 "

// Decryption info
static uint8_t wifi_iv[]  = { 0xBE, 0x98, 0x8A, 0xEC, 0x22, 0x75, 0x43, 0xF6, 0x92, 0xCA, 0xB1, 0xE7, 0xEC, 0x7B, 0x0A, 0xC0 };
static uint8_t wifi_key[]  = { 0x2E, 0x2C, 0x18, 0x3D, 0xBB, 0x77, 0x49, 0x3D, 0xB0, 0x8A };

// Max lengths
#define MAX_SSID_LEN                32
#define MAX_PASSWD_LEN              64

// Scan states
#define SCAN_INIT                   0
#define SCAN_START                  1
#define SCAN_MORE                   2

// Inactivity timeout
#define PROV_IDLE_CHECK             15
#define PROV_IDLE_TIMEOUT           (PROV_IDLE_CHECK * 10)

// Globals
static GMainLoop *main_loop;

// Characteristic properties
const char *read_write_props[] = { "read", "write", NULL };
const char *read_only_props[] = { "read", NULL };
const char *read_notify_props[] = { "read", "notify", NULL };
const char *write_only_props[] = { "write", NULL };

// Wifi config and status
static char wifiSSID[MAX_SSID_LEN + 1] = { 0 };
static char wifiAuthType[16] = { 0 };
static uint8_t wifiPasswd[MAX_PASSWD_LEN] = { 0 };
static int  wifiPasswdLen = 0;
static char wifiStatus[24] = { 0 };
static char *wifiScanResults = NULL;
static int wifiScanState = SCAN_INIT;
static char *wifiScanPtr = NULL;
static int wifiScanCount = 0;
static char wifiScanData[512];
static time_t wifiActivity;
static pthread_mutex_t activityLock;

// Last values to check for changes
static char lastSSID[MAX_SSID_LEN + 1] = { 0 };
static char lastAuthType[16] = { 0 };
static uint8_t lastPasswd[MAX_PASSWD_LEN] = { 0 };
static int  lastPasswdLen = 0;

// Signal handling
static gboolean sig_handler(GIOChannel *channel, GIOCondition cond,
                               gpointer user_data)
{
    static int done = 0;
    struct signalfd_siginfo si;
    int fd;

    if (cond & (G_IO_NVAL | G_IO_ERR | G_IO_HUP)) {
        return FALSE;
    }
    fd = g_io_channel_unix_get_fd(channel);
    if (read(fd, &si, sizeof(si)) != sizeof(si)) {
        return FALSE;
    }

    // Handle the signals we care about
    switch (si.ssi_signo) {
    case SIGINT:
    case SIGTERM:
      if (!done) {
          syslog(LOG_ERR, "Killing wifi provisioning...");

          // Kill main loop cleanly
          g_main_loop_quit(main_loop);
      }
      done = 1;
      break;
    }

    return TRUE;
}

static guint setup_signals(void)
{
    GIOChannel *ch;
    guint ret;
    sigset_t ss;
    int fd;

    // Set signal set - handle SIGINT and SIGTERM
    sigemptyset(&ss);
    sigaddset(&ss, SIGINT);
    sigaddset(&ss, SIGTERM);
    if (sigprocmask(SIG_BLOCK, &ss, NULL) < 0) {
        return 0;
    }
    fd = signalfd(-1, &ss, 0);
    if (fd < 0) {
        return 0;
    }

    // Setup channel
    ch = g_io_channel_unix_new(fd);
    g_io_channel_set_close_on_unref(ch, TRUE);
    g_io_channel_set_encoding(ch, NULL, NULL);
    g_io_channel_set_buffered(ch, FALSE);
    ret = g_io_add_watch(ch, G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_NVAL,
                            sig_handler, NULL);
    g_io_channel_unref(ch);

    return ret;
}


// DBUS proxy callback
static void proxy_cb(GDBusProxy *proxy, void *user_data)
{
    const char *intf;

    intf = g_dbus_proxy_get_interface(proxy);
    if (g_strcmp0(intf, GATT_MGR_IFACE)) {
        return;
    }
    register_app(proxy);
}


// Create Device Info Service
static char *create_device_info_service(DBusConnection *conn)
{
    char *path;
    char model[32];
    char hubID[32];
    char version[32];
    char hwVersion[32];

    path = register_service(conn, DEVICE_INFO_SERVICE_UUID);
    if (!path) {
        return NULL;
    }

    // Model number
    IRIS_getModel(model);
    if (!register_characteristic(conn, MODEL_NUMBER_UUID,
                                 (uint8_t *)model, strlen(model),
                                 read_only_props,
                                 NULL, NULL, 0, NULL,
                                 path, NULL, NULL)) {
        syslog(LOG_ERR, "Could not add Model Number characteristic");
        g_dbus_unregister_interface(conn, path, GATT_SERVICE_IFACE);
        g_free(path);
        return NULL;
    }

    // Serial number (HubID)
    IRIS_getHubID(hubID);
    if (!register_characteristic(conn, SERIAL_NUMBER_UUID,
                                 (uint8_t *)hubID, strlen(hubID),
                                 read_only_props,
                                 NULL, NULL, 0, NULL,
                                 path, NULL, NULL)) {
        syslog(LOG_ERR, "Could not add Serial Number characteristic");
        g_dbus_unregister_interface(conn, path, GATT_SERVICE_IFACE);
        g_free(path);
        return NULL;
    }

    // Firmware Version
    IRIS_getFirmwareVersion(version);
    if (!register_characteristic(conn, FIRMWARE_REV_UUID,
                                 (uint8_t *)version, strlen(version),
                                 read_only_props,
                                 NULL, NULL, 0, NULL,
                                 path, NULL, NULL)) {
        syslog(LOG_ERR, "Could not add Firmware Revision characteristic");
        g_dbus_unregister_interface(conn, path, GATT_SERVICE_IFACE);
        g_free(path);
        return NULL;
    }

    // Hardware Version
    snprintf(hwVersion, sizeof(hwVersion), "%d", IRIS_getHardwareVersion());
    if (!register_characteristic(conn, HARDWARE_REV_UUID,
                                 (uint8_t *)hwVersion, strlen(hwVersion),
                                 read_only_props,
                                 NULL, NULL, 0, NULL,
                                 path, NULL, NULL)) {
        syslog(LOG_ERR, "Could not add Hardware Revision characteristic");
        g_dbus_unregister_interface(conn, path, GATT_SERVICE_IFACE);
        g_free(path);
        return NULL;
    }

    // Manufacturer
    if (!register_characteristic(conn, MFG_NAME_UUID,
                                 (uint8_t *)MFG_NAME, strlen(MFG_NAME),
                                 read_only_props,
                                 NULL, NULL, 0, NULL,
                                 path, NULL, NULL)) {
        syslog(LOG_ERR, "Could not add Manufacturer Name characteristic");
        g_dbus_unregister_interface(conn, path, GATT_SERVICE_IFACE);
        g_free(path);
        return NULL;
    }

    return path;
}

// Update wifi activity
static void updateActivity()
{
    pthread_mutex_lock(&activityLock);
    wifiActivity = time(NULL);
    pthread_mutex_unlock(&activityLock);
}

// Callbacks to update items
static gboolean UpdateWifiScanResults(uint8_t **value, int *vlen,
                                      gboolean internal)
{
    struct stat st;
    char *ptr;

    // If the request is internal, return entire blob
    if (internal) {
        wifiScanState = SCAN_INIT;
    } else {
        updateActivity();
    }

    // Response depends on state
    switch (wifiScanState) {
    case SCAN_INIT:
        // Get size of scan file
        if (stat(WIFI_SCAN_FILE, &st) != -1) {
            if (st.st_size > 0) {
                FILE *f;

                f = fopen(WIFI_SCAN_FILE, "r");
                if (f != NULL) {
                    // Free existing buffer
                    if (wifiScanResults) {
                        free(wifiScanResults);
                    }

                    // Get buffer
                    wifiScanResults = malloc(st.st_size+1);
                    if (wifiScanResults != NULL) {
                        wifiScanResults[0] = '\0';
                        if (fread(wifiScanResults, 1, st.st_size, f) ==
                            st.st_size) {
                          wifiScanResults[st.st_size] = '\0';
                          wifiScanPtr = wifiScanResults;
                        }
                    }
                }
            }
        }

        // No data?
        if (wifiScanPtr == NULL) {
            *vlen = 0;
            *value = NULL;
            return FALSE;
        }

        // Internal? return entire block
        if (internal) {
            *vlen = strlen(wifiScanResults);
            *value = (uint8_t *)wifiScanResults;
            return FALSE;
        }

        // We fall through to START state
        wifiScanState = SCAN_START;

    case SCAN_START:
        // How many SSIDs?
        wifiScanCount = 0;
        ptr = wifiScanPtr;
        while ((ptr = strstr(ptr, SSID_KEY)) != NULL) {
            ptr += strlen(SSID_KEY);
            wifiScanCount++;
        }

        // 2 or fewer SSIDs?  Return whole block
        if (wifiScanCount <= 2) {
            *vlen = strlen(wifiScanResults);
            *value = (uint8_t *)wifiScanResults;
            // Reset state
            wifiScanState = SCAN_INIT;
            wifiScanPtr = NULL;
            return TRUE;
        }
        // We fall through to MORE state
        wifiScanState = SCAN_MORE;

    case SCAN_MORE:

        if (wifiScanCount <= 2) {
            snprintf(wifiScanData, sizeof(wifiScanData),
                     "{\"scanresults\":[%s", strstr(wifiScanPtr, SSID_KEY));
            wifiScanCount = 0;
        } else {
            char ssid1[128];
            char ssid2[128];
            char *ptr1, *ptr2;
            int len;

            ptr1 = strstr(wifiScanPtr, SSID_KEY);
            ptr2 = ptr1 + strlen(SSID_KEY);
            ptr2 = strstr(ptr2, SSID_KEY);
            while (*ptr2 != '}') ptr2--;
            len = (ptr2 - ptr1) + 1;
            strncpy(ssid1, ptr1, len);
            ssid1[len] = '\0';

            ptr1 = strstr(ptr2, SSID_KEY);
            ptr2 = ptr1 + strlen(SSID_KEY);
            ptr2 = strstr(ptr2, SSID_KEY);
            while (*ptr2 != '}') ptr2--;
            len = (ptr2 - ptr1) + 1;
            strncpy(ssid2, ptr1, len);
            ssid2[len] = '\0';
            wifiScanPtr = ptr2;
            snprintf(wifiScanData, sizeof(wifiScanData),
                     "{\"scanresults\":[%s, %s], \"more\":\"true\"}",
                     ssid1, ssid2);
            wifiScanCount -= 2;
        }
        *vlen = strlen(wifiScanData);
        *value = (uint8_t *)wifiScanData;

        // If no more ssids, back to init state
        if (wifiScanCount == 0) {
            wifiScanState = SCAN_INIT;
            wifiScanPtr = NULL;
        }
        return TRUE;

    default:
        *vlen = 0;
        *value = NULL;
        return FALSE;
    }
}

static gboolean UpdateWifiStatus(uint8_t **value, int *vlen, gboolean internal)
{
    FILE *f;

    // Update activity for external requests
    if (!internal) {
        updateActivity();
    }

    f = fopen(WIFI_STATUS_FILE, "r");
    if (f != NULL) {
        int len;

        wifiStatus[0] = '\0';
        fscanf(f, "%s", wifiStatus);
        fclose(f);

        // Status?
        len = strlen(wifiStatus);
        if (len > 0) {
            *vlen = len;
            *value = (uint8_t *)&wifiStatus[0];
            return TRUE;
        }
    }
    *vlen = 0;
    *value = NULL;
    return FALSE;
}


// Write config to filesystem once all pieces are available
void updateConfig()
{
    // All config must be present to update (and have changed)
    if ((wifiSSID[0] != '\0') && strcmp(wifiSSID, lastSSID) &&
        (wifiAuthType[0] != '\0') && strcmp(wifiAuthType, lastAuthType) &&
        (wifiPasswdLen != 0) && memcmp(wifiPasswd, lastPasswd, wifiPasswdLen)) {
        FILE *f;

        // Open config file
        f = fopen(TEST_WIFI_CFG_FILE, "w");
        if (f != NULL) {
            struct AES_ctx ctx;
            char data[512];
            unsigned char key[16];
            char macAddr[32];
            unsigned long long macBytes;

            // Need our MAC address for key
            IRIS_getMACAddr1(data);
            snprintf(macAddr, sizeof(macAddr), "%c%c%c%c%c%c%c%c%c%c%c%c",
                     data[0], data[1], data[3], data[4], data[6], data[7],
                     data[9], data[10], data[12], data[13], data[15], data[16]);
            macBytes = strtoull(macAddr, NULL, 16);

            // Decode password
            key[0] = wifi_key[0];
            key[1] = wifi_key[1];
            key[2] = macBytes & 0x00FF;
            key[3] = wifi_key[2];
            key[4] = (macBytes >> 8) & 0x00FF;
            key[5] = wifi_key[3];
            key[6] = (macBytes >> 16) & 0x00FF;
            key[7] = wifi_key[4];
            key[8] = wifi_key[5];
            key[9] = (macBytes >> 24) & 0x00FF;
            key[10] = wifi_key[6];
            key[11] = (macBytes >> 32) & 0x00FF;
            key[12] = wifi_key[7];
            key[13] = (macBytes >> 40) & 0x00FF;
            key[14] = wifi_key[8];
            key[15] = wifi_key[9];
            AES_init_ctx_iv(&ctx, key, wifi_iv);
            AES_CBC_decrypt_buffer(&ctx, wifiPasswd, wifiPasswdLen);

            // There should only be printable characters in the result
            //  so trim off padding
            for (int i = 0; i < wifiPasswdLen; i++) {
                if (!isprint(wifiPasswd[i])) {
                    wifiPasswd[i] = '\0';
                    break;
                }
            }

            // Write configuration
            snprintf(data, sizeof(data), "ssid: %s\nsecurity: %s\nkey: %s\n",
                     wifiSSID, wifiAuthType, wifiPasswd);
            fprintf(f, data);
            fclose(f);
            syslog(LOG_INFO, "Updated Wifi config file...");

            // Update values
            strncpy(lastSSID, wifiSSID, sizeof(lastSSID));
            strncpy(lastAuthType, wifiAuthType, sizeof(lastAuthType));
            lastPasswdLen = wifiPasswdLen;
            memcpy(lastPasswd, wifiPasswd, wifiPasswdLen);
        } else {
            syslog(LOG_ERR, "Couldn't open Wifi config file!");
	}
    }
}

// Callbacks to validate data and record new value
static gboolean ValidateWifiSSID(const uint8_t *value, int vlen)
{
    // Update activity
    updateActivity();

    // Check for max size
    if (vlen > MAX_SSID_LEN) {
        return FALSE;
    }
    strncpy(wifiSSID, (char *)value, vlen);
    wifiSSID[vlen] = '\0';
    syslog(LOG_INFO, "Wifi config - new SSID: %s", wifiSSID);
    updateConfig();
    return TRUE;
}

static gboolean ValidateWifiAuthType(const uint8_t *value, int vlen)
{
    char *auth_type = (char *)value;

    // Update activity
    updateActivity();

    // Only certain values are allowed
    if (((strncmp(auth_type, "None", 4) == 0) && (vlen == 4)) ||
        ((strncmp(auth_type, "WEP", 3) == 0) && (vlen == 3)) ||
        ((strncmp(auth_type, "WPA-PSK", 7) == 0) && (vlen == 7)) ||
        ((strncmp(auth_type, "WPA2-PSK", 8) == 0) && (vlen == 8))) {
        char *ptr;

        strncpy(wifiAuthType, auth_type, vlen);
        wifiAuthType[vlen] = '\0';
        // We use '-' in the scan results security WPA names, but '_' in the
        //  agent config.  Replace as it's too late to change this
        if ((ptr = strchr(wifiAuthType, '-')) != NULL) {
            *ptr = '_';
        }
        syslog(LOG_INFO, "Wifi config - new auth type: %s", wifiAuthType);
        updateConfig();
        return TRUE;
    }
    return FALSE;
}

static gboolean ValidateWifiPasswd(const uint8_t *value, int vlen)
{
    // Update activity
    updateActivity();

    // Check for max size
    if (vlen > MAX_PASSWD_LEN) {
        return FALSE;
    }
    memcpy(wifiPasswd, value, vlen);
    wifiPasswdLen = vlen;
    syslog(LOG_INFO, "Wifi config - password updated");
    updateConfig();
    return TRUE;
}



// Create Wifi Config Service
static char *create_wifi_cfg_service(DBusConnection *conn)
{
    char *path = NULL;

    path = register_service(conn, WIFI_CFG_SERVICE_UUID);
    if (!path) {
        return NULL;
    }

    // Scan results
    if (!register_characteristic(conn, WIFI_SCAN_RESULTS_UUID,
                                 (uint8_t *)"", 0, // Filled in by scan...
                                 read_only_props,
                                 WIFI_SCAN_RESULTS_UUID,
                                 (uint8_t *)SCAN_RESULTS_DESC,
                                 strlen(SCAN_RESULTS_DESC),
                                 read_only_props,
                                 path, NULL, &UpdateWifiScanResults)) {
        syslog(LOG_ERR, "Could not add Wifi Scan Results characteristic");
        g_dbus_unregister_interface(conn, path, GATT_SERVICE_IFACE);
        g_free(path);
        return NULL;
    }

    // Supported Modes
    if (!register_characteristic(conn, WIFI_SUP_MODES_UUID,
                                 (uint8_t *)SUPPORTED_MODES,
                                 strlen(SUPPORTED_MODES),
                                 read_only_props,
                                 WIFI_SUP_MODES_UUID,
                                 (uint8_t *)SUPPORTED_MODES_DESC,
                                 strlen(SUPPORTED_MODES_DESC),
                                 read_only_props,
                                 path, NULL, NULL)) {
        syslog(LOG_ERR, "Could not add Wifi Scan Results characteristic");
        g_dbus_unregister_interface(conn, path, GATT_SERVICE_IFACE);
        g_free(path);
        return NULL;
    }

    // Supported Frequencies
    if (!register_characteristic(conn, WIFI_SUP_FREQS_UUID,
                                 (uint8_t *)SUPPORTED_FREQS,
                                 strlen(SUPPORTED_FREQS),
                                 read_only_props,
                                 WIFI_SUP_FREQS_UUID,
                                 (uint8_t *)SUPPORTED_FREQS_DESC,
                                 strlen(SUPPORTED_FREQS_DESC),
                                 read_only_props,
                                 path, NULL, NULL)) {
        syslog(LOG_ERR, "Could not add Wifi Scan Results characteristic");
        g_dbus_unregister_interface(conn, path, GATT_SERVICE_IFACE);
        g_free(path);
        return NULL;
    }

    // Current mode
    if (!register_characteristic(conn, WIFI_CUR_MODE_UUID,
                                 (uint8_t *)CURRENT_MODE,
                                 strlen(CURRENT_MODE),
                                 read_write_props,
                                 WIFI_CUR_MODE_UUID,
                                 (uint8_t *)CURRENT_MODE_DESC,
                                 strlen(CURRENT_MODE_DESC),
                                 read_only_props,
                                 path, NULL, NULL)) {
        syslog(LOG_ERR, "Could not add Wifi Current Mode characteristic");
        g_dbus_unregister_interface(conn, path, GATT_SERVICE_IFACE);
        g_free(path);
        return NULL;
    }

    // Current frequency
    if (!register_characteristic(conn, WIFI_CUR_FREQ_UUID,
                                 (uint8_t *)CURRENT_FREQ,
                                 strlen(CURRENT_FREQ),
                                 read_write_props,
                                 WIFI_CUR_FREQ_UUID,
                                 (uint8_t *)CURRENT_FREQ_DESC,
                                 strlen(CURRENT_FREQ_DESC),
                                 read_only_props,
                                 path, NULL, NULL)) {
        syslog(LOG_ERR,
                "Could not add Wifi Current Frequency characteristic");
        g_dbus_unregister_interface(conn, path, GATT_SERVICE_IFACE);
        g_free(path);
        return NULL;
    }

    // Config SSID
    if (!register_characteristic(conn, WIFI_CFG_SSID_UUID,
                                 (uint8_t *)"", 0, // Filled in by user..
                                 read_write_props,
                                 WIFI_CFG_SSID_UUID,
                                 (uint8_t *)CONFIG_SSID_DESC,
                                 strlen(CONFIG_SSID_DESC),
                                 read_only_props,
                                 path, &ValidateWifiSSID, NULL)) {
        syslog(LOG_ERR,
                "Could not add Wifi Config SSID characteristic");
        g_dbus_unregister_interface(conn, path, GATT_SERVICE_IFACE);
        g_free(path);
        return NULL;
    }

    // Authentication type
    if (!register_characteristic(conn, WIFI_AUTH_TYPE_UUID,
                                 (uint8_t *)"", 0, // Filled in by user..
                                 read_write_props,
                                 WIFI_AUTH_TYPE_UUID,
                                 (uint8_t *)AUTH_TYPE_DESC,
                                 strlen(AUTH_TYPE_DESC),
                                 read_only_props,
                                 path, &ValidateWifiAuthType, NULL)) {
        syslog(LOG_ERR,
                "Could not add Wifi Authentication Type characteristic");
        g_dbus_unregister_interface(conn, path, GATT_SERVICE_IFACE);
        g_free(path);
        return NULL;
    }

    // Encryption type
    if (!register_characteristic(conn, WIFI_ENC_TYPE_UUID,
                                 (uint8_t *)"", 0, // Filled in by user..
                                 read_write_props,
                                 WIFI_ENC_TYPE_UUID,
                                 (uint8_t *)ENC_TYPE_DESC,
                                 strlen(ENC_TYPE_DESC),
                                 read_only_props,
                                 path, NULL, NULL)) {
        syslog(LOG_ERR,
                "Could not add Wifi Encryption Type characteristic");
        g_dbus_unregister_interface(conn, path, GATT_SERVICE_IFACE);
        g_free(path);
        return NULL;
    }

    // Config password
    if (!register_characteristic(conn, WIFI_CFG_PASSWD_UUID,
                                 (uint8_t *)"", 0, // Filled in by user..
                                 read_write_props,
                                 WIFI_CFG_PASSWD_UUID,
                                 (uint8_t *)CONFIG_PASSWD_DESC,
                                 strlen(CONFIG_PASSWD_DESC),
                                 read_only_props,
                                 path, &ValidateWifiPasswd, NULL)) {
        syslog(LOG_ERR,
                "Could not add Wifi Config Password characteristic");
        g_dbus_unregister_interface(conn, path, GATT_SERVICE_IFACE);
        g_free(path);
        return NULL;
    }

    // Status
    if (!register_characteristic(conn, WIFI_STATUS_UUID,
                                 (uint8_t *)DEFAULT_STATUS,
                                 strlen(DEFAULT_STATUS),
                                 read_only_props,
                                 WIFI_STATUS_UUID,
                                 (uint8_t *)STATUS_DESC,
                                 strlen(STATUS_DESC),
                                 read_only_props,
                                 path, NULL, &UpdateWifiStatus)) {
        syslog(LOG_ERR,
                "Could not add Wifi Status characteristic");
        g_dbus_unregister_interface(conn, path, GATT_SERVICE_IFACE);
        g_free(path);
        return NULL;
    }

    return path;
}


// Register all the services we care about
static void create_services(DBusConnection *conn, GSList *services)
{
    char *path;

    // Device Information Service
    path = create_device_info_service(conn);
    if (!path) {
        syslog(LOG_ERR, "Unable to register Device Info Service");
    } else {
        services = g_slist_prepend(services, path);
        syslog(LOG_INFO, "Registered Device Info Service: %s", path);
    }

    // Wifi Config Service
    path = create_wifi_cfg_service(conn);
    if (!path) {
        syslog(LOG_ERR, "Unable to register Wifi Config Service");
    } else {
        services = g_slist_prepend(services, path);
        syslog(LOG_INFO, "Registered Wifi Config Service: %s", path);
    }
}

static gboolean timeout_handler(gpointer user_data)
{
    // If there was recent activity, restart timeout
    pthread_mutex_lock(&activityLock);
    if (wifiActivity + PROV_IDLE_TIMEOUT >= time(NULL)) {
        pthread_mutex_unlock(&activityLock);
        return TRUE;
    }
    pthread_mutex_unlock(&activityLock);

    // No activity, kill provisioning
    g_main_loop_quit(main_loop);
    return FALSE;
}

static void usage(char *name)
{
    fprintf(stderr, "\nusage: %s [options]\n"
            "  options:\n"
            "    -h           Print this message\n"
            "    -v           Get current BLE firmware version and exit\n"
            "                 Start BLE provisioning daemon\n"
            "\n", name);
}

int main(int argc, char *argv[])
{
    DBusConnection *conn = NULL;
    GDBusClient *client;
    guint sigs = 0;
    GSList *services = NULL;
    char cmd[256];
    char macAddr[32];
    char deviceName[64];
    char staticAddr[32] = { 0 };
    FILE *f;
    int  c;
    int  getVersion = 0;
    char line[256];

    /* Parse options */
    opterr = 0;
    while ((c = getopt(argc, argv, "hv")) != -1)
    switch (c) {
    case 'h':
        usage(argv[0]);
        return 0;
    case 'v':
        getVersion = 1;
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

    // Setup syslog
    openlog(SYSLOG_IDENT, (LOG_CONS | LOG_PID | LOG_PERROR), LOG_USER);

    // If we just want to return the version, no need for this setup
    if (getVersion) {
        goto read_version;
    }

    // Setup signal handlers
    sigs = setup_signals();
    if (sigs == 0) {
        syslog(LOG_ERR, "Error setting up signal handlers!");
        exit(-errno);
    }

    // Create activity mutex
    if (pthread_mutex_init(&activityLock, NULL) != 0) {
        syslog(LOG_ERR, "Error creating activity lock!");
        exit(-1);
    }
    wifiActivity = time(NULL);

    // Create DBUS connection
    conn = g_dbus_setup_bus(DBUS_BUS_SYSTEM, NULL, NULL);
    g_dbus_attach_object_manager(conn);

    // Create GATT services we handle
    create_services(conn, services);

    // Set display name - we use main MAC address
    IRIS_getMACAddr1(macAddr);
    // Skip colons
    snprintf(deviceName, sizeof(deviceName) - 1,
             "Iris_Hub_%c%c%c%c%c%c%c%c%c%c%c%c",
             macAddr[0], macAddr[1], macAddr[3], macAddr[4],
             macAddr[6], macAddr[7], macAddr[9], macAddr[10],
             macAddr[12], macAddr[13], macAddr[15], macAddr[16]);
    snprintf(cmd, sizeof(cmd) - 1, SET_ALIAS_CMD, deviceName);
    system(cmd);

 read_version:
    // Bring up BLE port
    system(SET_PORT_UP_CMD);

    // Get local BLE firmware version
    snprintf(cmd, sizeof(cmd) - 1, GET_LOCAL_VERSION_CMD);
    f = popen(cmd, "r");
    if (f) {
        while (fgets(line, sizeof(line), f) != NULL) {
            // The third line has the version data
            char * ptr;
            if ((ptr = strstr(line, LOCAL_VERSION_HDR)) != NULL) {
                char *token;
                int index = 0;
                long int major = 0, minor = 0, point = 0;

		// Parse version data
                ptr += strlen(LOCAL_VERSION_HDR);
                token = strtok(ptr, " ");
                while (token != NULL) {
                    if (index == 4) major = strtol(token, NULL, 16);
                    if (index == 5) minor = strtol(token, NULL, 16);
                    if (index == 6) point = strtol(token, NULL, 16);
                    token = strtok(NULL, " ");
                    index++;
                }

                // Record firmware version in log and file
                syslog(LOG_INFO, "BLE Firmware Version: %ld.%ld.%ld",
                       major, minor, point);
                snprintf(cmd, sizeof(cmd) - 1, "echo %ld.%ld.%ld > %s",
                         major, minor, point, BLE_FW_VER_FILE);
                system(cmd);
                break;
            }
        }
        pclose(f);
    }

    // If we just wanted to return the version, exit now
    if (getVersion) {
        goto exit;
    }

    // Get Zephyr static address data
    snprintf(cmd, sizeof(cmd) - 1, GET_STATIC_ADDRS_CMD);
    f = popen(cmd, "r");
    if (f) {
        while (fgets(line, sizeof(line), f) != NULL) {
	    // The third line has the static address data
	    char * ptr;
	    if ((ptr = strstr(line, STATIC_ADDR_HDR)) != NULL) {
		ptr += strlen(STATIC_ADDR_HDR);
		strncpy(staticAddr, ptr, sizeof(staticAddr));
		staticAddr[17] = '\0';
		break;
	    }
	}
	pclose(f);
    }

    // Set static address
    snprintf(cmd, sizeof(cmd) - 1, SET_STATIC_ADDR_CMD, staticAddr);
    system(cmd);

    // Make discoverable, for 1hr
    snprintf(cmd, sizeof(cmd) - 1, SET_DISCOVERABLE_CMD, "true");
    system(cmd);
    snprintf(cmd, sizeof(cmd) - 1, SET_DISC_TIMEOUT_CMD, 3600);
    system(cmd);

    // Set advertising params
    system(SET_ADV_PARAMS_CMD);

    // Set Advertising data
    system(SET_ADV_DATA_CMD);

    // Set Scan Response Data (Iris_Hub_XXXXXXXXXXXX)
    snprintf(cmd, sizeof(cmd) - 1, SET_ADV_RESP_DATA_CMD,
             macAddr[0], macAddr[1], macAddr[3], macAddr[4],
             macAddr[6], macAddr[7], macAddr[9], macAddr[10],
             macAddr[12], macAddr[13], macAddr[15], macAddr[16]);
    system(cmd);

    // Enable advertising
    system(SET_ADV_ENABLE_CMD);

    // Set up DBUS client and handlers
    client = g_dbus_client_new(conn, "org.bluez", "/");
    g_dbus_client_set_proxy_handlers(client, proxy_cb, NULL, NULL, NULL);

    // Create idle timeout handler
    g_timeout_add_seconds(PROV_IDLE_CHECK, timeout_handler, NULL);

    // Handle processing
    main_loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(main_loop);

    // Loop is done, must have been killed - clean up
    g_dbus_client_unref(client);
    g_source_remove(sigs);
    g_slist_free_full(services, g_free);
    dbus_connection_unref(conn);
    pthread_mutex_destroy(&activityLock);

 exit:
    // Disable BLE port
    system(SET_PORT_DOWN_CMD);

    return 0;
}
