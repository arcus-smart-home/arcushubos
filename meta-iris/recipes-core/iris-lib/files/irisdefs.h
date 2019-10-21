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

#ifndef IRIS_UTILS_H
#define IRIS_UTILS_H

//
//  Global defines
//

// Files used in /tmp

// Location of LED mode file - write to this to change LED setting
#define LED_MODE_FILE     "/tmp/ledMode"

// Last LED mode setting file
#define LAST_LED_MODE_FILE "/tmp/lastLedMode"

// Build version information
#define VERSION_FILE      "/tmp/version"

// Firmware install file
#define FW_INSTALL_FILE   "/tmp/fw_image.bin"

// Battery mode enabled - file contains time we last started to run on battery
#define BTRY_ON_FILE      "/tmp/battery_on"

// Battery level - on supporting hardware, level from 0 - 100
#define BTRY_LEVEL_FILE   "/tmp/battery_level"

// Battery voltage - on supporting hardware, current battery voltage
#define BTRY_VOLTAGE_FILE "/tmp/battery_voltage"

// Battery charge state - on supporting hardware
#define CHARGE_STATE_FILE "/tmp/charge_state"

// Battery charge level - on supporting hardware
#define CHARGE_LEVEL_FILE "/tmp/charge_level"

// Debug mode is enabled - SSH enabled, etc.
#define DEBUG_ENABLED_FILE "/tmp/debug"

// Agent configuration file - copied from debug dongle
#define AGENT_CFG_FILE    "/tmp/agent_cfg"

// USB over-current enabled - file contains last time condition began
#define USB0_OC_ON_FILE   "/tmp/usb0_oc_on"
#define USB1_OC_ON_FILE   "/tmp/usb1_oc_on"

// NFC tag file
#define NFC_TAG_FILE      "/tmp/nfc_tag"

// BLE Firmware Version file
#define BLE_FW_VER_FILE   "/tmp/bleFwVersion"

// Wifi status
#define WIFI_STATUS_FILE  "/tmp/wifiStatus"
#define WIFI_TEST_FILE    "/tmp/wifiTestResult"
#define WIFI_SCAN_FILE    "/tmp/wifiScanResults"
#define WIFI_SCAN_CMD     "/usr/bin/wifi_scan -t"

typedef enum
{
    DISCONNECTED = 0,
    CONNECTING,
    BAD_SSID,
    BAD_PASS,
    NO_INTERNET,
    NO_SERVER,
    CONNECTED
} WifiStatusType;

// Iris Button (hubv3) pushed time
#define IRIS_BTN_PUSHED_FILE "/tmp/irisButtonPushed"

// Where we mount USB drives
#define MEDIA_MOUNT_DIR   "/run/media"

// Max depth to search for files on USB drives
#define MAX_FIND_DEPTH    2

// Manufacturing data
#define MODEL_FILE        "/tmp/mfg/config/model"
#define CUSTOMER_FILE     "/tmp/mfg/config/customer"
#define BATCH_NO_FILE     "/tmp/mfg/config/batchNo"
#define HUB_ID_FILE       "/tmp/mfg/config/hubID"
#define HW_VER_FILE       "/tmp/mfg/config/hwVer"
#define MAC_ADDR1_FILE    "/tmp/mfg/config/macAddr1"
#define MAC_ADDR2_FILE    "/tmp/mfg/config/macAddr2"

// HW Version information
#define HW_VERSION_INITIAL               1
#define HW_VERSION_WITH_FLOWCONTROL      2
#define HW_VERSION_WITH_BATTERY_VOLTAGE  3

// Hubv3 version information
#define HWV3_VERION_WITH_NFC             2

// GPIOs vary by plaform and are setup in the irisinit script (for
//  the most part if used for more than one platform)

// GPIO button control
#define BUTTON_GPIO_VALUE_FILE  "/tmp/io/resetBtnValue"
#define BUTTON_GPIO_EDGE_FILE   "/tmp/io/resetBtnEdge"
#define BUTTON_HELD_VALUE       '0'
#define BUTTON_DEFAULT_EDGE     "both"

// RADIO resets
#define ZIGBEE_RESET_GPIO_VALUE_FILE "/tmp/io/zigbeeReset"
#define ZWAVE_RESET_GPIO_VALUE_FILE  "/tmp/io/zwaveReset"
#define BLE_RESET_GPIO_VALUE_FILE    "/tmp/io/bleReset"
#define LTE_RESET_GPIO_VALUE_FILE    "/tmp/io/lteReset"

// PWM Sounder control
#define PWM_PERIOD              "/tmp/io/pwmPeriod"
#define PWM_DUTY_CYCLE          "/tmp/io/pwmDutyCycle"
#define PWM_ENABLE              "/tmp/io/pwmEnable"

// Platform specific GPIO related defines
#if beaglebone
// USB Over-current signals
#define USB0_OC_GPIO_VALUE_FILE "/sys/class/gpio/gpio116/value"
#define USB1_OC_GPIO_VALUE_FILE "/sys/class/gpio/gpio58/value"

// Battery hold gpio
#define BTRY_HOLD_GPIO_VALUE_FILE "/sys/class/gpio/gpio117/value"

// Battery voltage ADC port
#define BTRY_VOLTAGE_VALUE_FILE "/sys/bus/iio/devices/iio:device0/in_voltage6_raw"
#endif

#if imxdimagic
// IRIS button on front of v3 hub
#define IRIS_BUTTON_GPIO_VALUE_FILE  "/tmp/io/irisBtnValue"
#define IRIS_BUTTON_GPIO_EDGE_FILE   "/tmp/io/irisBtnEdge"
#define IRIS_BUTTON_HELD_VALUE       '0'
#define IRIS_BUTTON_DEFAULT_EDGE     "both"

// Power down button (recessed on back of v3 hub)
#define PWR_DOWN_BTN_GPIO_VALUE_FILE "/tmp/io/pwrDownBtnValue"
#define PWR_DOWN_BTN_GPIO_EDGE_FILE  "/tmp/io/pwrDownBtnEdge"
#define PWR_DOWN_BTN_HELD_VALUE      '0'
#define PWR_DOWN_BTN_DEFAULT_EDGE    "both"

// Power control GPIOs for radio subsystems
#define PWR_EN_ZWAVE_VALUE_FILE      "/tmp/io/zwavePower"
#define PWR_EN_ZIGBEE_VALUE_FILE     "/tmp/io/zigbeePower"
#define PWR_EN_BLE_VALUE_FILE        "/tmp/io/blePower"
#define PWR_EN_LTE_VALUE_FILE        "/tmp/io/ltePower"

// USB port power
#define PWR_EN_USB_VALUE_FILE        "/tmp/io/usbPower"

// NFC device
#define NFC_DEV                      "/dev/spidev1.0"
#endif

/* Button press durations */
#define BUTTON_REBOOT_HOLD             1
#define BUTTON_SOFT_RESET_HOLD         10
#define BUTTON_FACTORY_DEFAULT_HOLD    20

// HW Watchdog
#define HW_WATCHDOG_DEV         "/dev/watchdog0"

// LED control application - LED ring vs. regular LEDs
#if imxdimagic
#define LED_CTRL_APP            "/usr/bin/ringctrl"
#else
#define LED_CTRL_APP            "/usr/bin/ledctrl"
#endif

// Storage for firmware public key
#define BUILD_PUBLIC_KEY   "/etc/.ssh/iris-fwsign.pub"
#define HUB_CA_CERT        "/etc/.ssh/iris-hubsign.chained.crt"

// Mfg partition info
#define MFG_MOUNT_DEV      "/dev/mfg"
#define MFG_MOUNT_DIR      "/mfg"
#define MFG_MOUNT_FSTYPE   "ext2"
#define MFG_CERTS_DIR      "/mfg/certs"
#define MFG_KEYS_DIR       "/mfg/keys"
#define MFG_MAC1_FILE      "/mfg/config/macAddr1"

// File update info - hard-coded for now to our Artifactory server
#define UPDATE_SERVER_URL  "https://eyerisbuild.cloudapp.net/artifactory/hubos/"
#define UPDATE_USERNAME    "hubos"
#define UPDATE_PASSWORD    "z4eYWeUGXSUXjABEXTPw42reLVm88hag"
#define UPDATE_VER_FILE    "hubOS_latest.txt"
#define FIRMWARE_PREFIX    "hubOS"

// Default image build info
#if beaglebone
#define DEFAULT_MODEL      "IH200"
#else
// We default to the v3 hub with LTE support, image works on wifi-only
//  version as well
#define DEFAULT_MODEL      "IH304"
#endif
#define DEFAULT_CUSTOMER   "IRIS"
#define DEFAULT_PREFIX     "hubOS"
#define CUSTOMER_ALL       "ALL"

// SSH Init script
#define SSH_INIT_SCRIPT         "/etc/init.d/dropbear"
#define SSH_BIN_FILE            "/usr/sbin/dropbear"
#define SSH_PID_FILE            "/var/run/dropbear.pid"
#define SSH_KEY_FILE            "/data/config/dropbear/dropbear_rsa_host_key"

// Binaries we look for to tell what build we are using
#define JAVA_BIN_FILE           "/usr/bin/java"
#define SERIALIZE_BIN_FILE      "/usr/bin/serialize"
#define IRIS_AGENT_BIN_FILE     "/usr/bin/irisagentd"

// ifplugd defines
#define IFPLUGD_BIN_FILE        "/usr/sbin/ifplugd"
#define IFPLUGD_PROCESS_NAME    "ifplugd"
#define IFPLUGD_START_SCRIPT    "/etc/init.d/ifplugd start"
#define UDHCPC_TERMINATE        "kill -9 $(cat /var/run/udhcpc.eth0.pid)"
#define UDHCPC_WIFI_TERMINATE   "kill -9 $(cat /var/run/udhcpc.wlan0.pid)"

// Serial ports we use
#define ZWAVE_UART              "/dev/ttyO1"
#define ZIGBEE_UART             "/dev/ttyO2"
#define BLE_UART                "/dev/ttyO4"

// Serial mode
#define FLOW_CONTROL_NONE       0
#define FLOW_CONTROL_XONXOFF    1
#define FLOW_CONTROL_RTSCTS     2

// Sound files
#define REBOOT_SOUND_FILE       "/home/root/sounds/reboot_sound.txt"
#define RESET_SOUND_FILE        "/home/root/sounds/reset_sound.txt"

// App to play audio files (on platforms that support it)
#define AUDIO_APP               "/usr/bin/play"

// Wifi interface, if available
#define WIFI_IF                 "wlan0"
#define ETH_IF                  "eth0"
#define CHECK_IF_STATUS         "ip link show %s | grep \"[<,]LOWER_UP[,>]\""
#define CHECK_IF_IP_UP          "ifconfig %s | grep \"inet addr\""
#define WIFI_RESTART_CMD        "/usr/bin/wifi_start %s &"
#define WIFI_TEST_CMD           "/usr/bin/wifi_test %s %s"
#define WIFI_STOP_CMD           "ifdown %s; ifconfig %s up"
#define WIFI_PROV_DAEMON        "wifi_prov"
#define TEST_RESOLVE_HOST       "google.com"

// Files used for agent control of LTE
#define LTE_CONTROL_FILE        "/tmp/lteControl"
#define LTE_STATUS_FILE         "/tmp/backupStatus"
#define LTE_DONGLE_FILE         "/tmp/lte_dongle"
#define LTE_INTF_FILE           "/tmp/lte_interface"
#define LTE_INFO_FILE           "/tmp/lte_info"
#define LTE_NET_STATUS_FILE     "/tmp/lte_status"

//
// Configuration files saved in /data/config
//

// Data config directory
#define DATA_CONFIG_DIR         "/data/config"

// If present, will skip image update checking in background
#define UPDATE_SKIP_FILE        "/data/config/update_skip"

// If present, allows console port login in release image
#define ENABLE_CONSOLE_FILE     "/data/config/enable_console"

// Timestamp file
#define TIMESTAMP_FILE          "/data/config/timestamp"

// Dropbear config directory
#define DROPBEAR_CONFIG_DIR     "/data/config/dropbear"

// Maximum battery voltage, for tracking of replaceable batteries
#define MAX_BATTERY_VOLTAGE     "/data/config/max_voltage"

// Wireless configuration
#define WIFI_CFG_FILE           "/data/config/wifiCfg"
#define TEST_WIFI_CFG_FILE      "/tmp/testWifiCfg"
#define PROVISIONED_FILE        "/data/config/provisioned"


//
// Other items in /data
//

// Data log directory
#define DATA_LOG_DIR            "/data/log"

// Data iris directory - agent saves database and logs here
#define DATA_IRIS_DIR           "/data/iris"

// Data jre libs directory - we unpack jre jar files here to save image space
#define DATA_JRE_LIBS_DIR       "/data/jre/libs"

// Agent directory
#define AGENT_ROOT_DIR          "/data/agent"

// Agent libs directory - agent saves runtime libraries here
#define AGENT_LIBS_DIR          "/data/agent/libs"

// File to signal agent to soft-reset itself on next reboot
#define AGENT_SOFT_RESET        "/data/agent/soft_reset"

// File to signal agent a factory default has occurred
#define AGENT_FACTORY_DEFAULT   "/data/agent/factory_reset"

//
//  Install error codes
//

// Install was successful
#define INSTALL_SUCCESS           0

// Error in arguments passed to command
#define INSTALL_ARG_ERR           1

// Error downloading file
#define INSTALL_DOWNLOAD_ERR      2

// Error opening firmware file
#define INSTALL_FILE_OPEN_ERR     3

// Error creating memory map
#define INSTALL_MEMORY_ERR        4

// General decrypt error - likely a resource issue (opening tmp file, etc)
#define INSTALL_DECRYPT_ERR       5

// Error in header signature
#define INSTALL_HDR_SIG_ERR       6

// Wrong model information for this platform
#define INSTALL_MODEL_ERR         7

// Wrong customer information for this platform
#define INSTALL_CUSTOMER_ERR      8

// Error in firmware decryption
#define INSTALL_FW_DECRYPT_ERR    9

// Error in file SHA256 checksum
#define INSTALL_CKSUM_ERR         10

// Firmware install already in process
#define INSTALL_IN_PROCESS        11

// Error in firmware archive unpack
#define INSTALL_UNPACK_ERR        12

// Error in archive checksum comparision
#define INSTALL_ARCHIVE_CKSUM_ERR 13

// Error mounting partition during install
#define INSTALL_MOUNT_ERR         14

// Error installing Zwave firmware
#define INSTALL_ZWAVE_ERR         15

// Error installing Zigbee firmware
#define INSTALL_ZIGBEE_ERR        16

// Error installing BLE firmware
#define INSTALL_BLE_ERR           17


// The following errors force a reboot
#define INSTALL_HARD_ERRORS       20

// Error installing u-boot files
#define INSTALL_UBOOT_ERR         21

// Error installing kernel files
#define INSTALL_KERNEL_ERR        22

// Error installing root-fs files
#define INSTALL_ROOTFS_ERR        23

#endif //IRIS_UTILS_H
