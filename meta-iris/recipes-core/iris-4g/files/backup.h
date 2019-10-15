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

#include <termios.h>
#include <irisdefs.h>

// Huawei E3372h, which has a fixed subnet (192.168.8.x)
#define LTE_HUAWEI_MODEM_ID     "12d1:14dc"
#define LTE_HUAWEI_MODEM_SUBNET "192.168.8"

// QuecTel EC25-A mPCIe module - not NAT'd
#define LTE_QUECTEL_MODEM_ID    "2c7c:0125"
#define LTE_QUECTEL_DAEMON      "quectel-CM"

// ZTEWelink ME3630 - presents usb0 interface
#define LTE_ZTEWELINK_MODEM_ID  "19d2:1476"
#define LTE_ZTEWELINK_AT_PORT   "/dev/ttyUSB1"
#define LTE_ZTEWELINK_AT_SPEED  B115200
#define LTE_ZTEWELINK_AT_FLOW   FLOW_CONTROL_RTSCTS
#define LTE_ZTEWELINK_AT_SETUP  "AT+CGDCONT=1,\"IP\",\"irisbylowes.com\"\n"
#define LTE_ZTEWELINK_AT_START  "AT+ZECMCALL=1\n"
#define LTE_ZTEWELINK_AT_STOP   "AT+ZECMCALL=0\n"
#define LTE_ZTEWELINK_AT_IMEI   "AT+CGSN\n"
#define LTE_ZTEWELINK_AT_IMSI   "AT+CIMI\n"
#define LTE_ZTEWELINK_AT_ICCID  "AT+ZGETICCID\n"
#define LTE_ZTEWELINK_AT_MSISDN "AT+CNUM\n"
#define LTE_ZTEWELINK_AT_SIGNAL "AT+CSQ\n"
#define LTE_ZTEWELINK_AT_INFO   "AT^SYSINFO\n"
#define LTE_ZTEWELINK_AT_RESET  "AT+ZRST\n"
#define LTE_ZTEWELINK_AT_ZSWITCH_CHECK "AT+ZSWITCH?\n"
#define LTE_ZTEWELINK_AT_ZSWITCH_SET "AT+ZSWITCH=L\n"

// DNS config
#define PRI_DNS_FILE     "/tmp/pri_resolv.conf"
#define BKUP_DNS_FILE    "/tmp/bkup_resolv.conf"
#define ORIG_DNS_FILE    "/var/run/resolv.conf"

// Gateway config
#define PRI_GW_FILE      "/tmp/pri_gw.conf"
#define BKUP_GW_FILE     "/tmp/bkup_gw.conf"
#define PRI_GW_AVAIL_FILE "/tmp/primary_available"

// Interface defines
#define PRIMARY_ETH_IF   "eth0"
#define PRIMARY_WLAN_IF  "wlan0"
#define BACKUP_ETH_IF    "eth1"
#define BACKUP_USB_IF    "usb0"
#define BACKUP_WWAN_IF   "wwan0"
#define BACKUP_CHECK_IF_STATUS "ip link show %s | grep \"[<,]UP[,>]\""

// Use a smaller MTU on LTE interface to avoid issues
#define BACKUP_MTU       "1400"

// DHCP client we use
#define DHCP_CLIENT      "udhcpc"

// Use 'ifup' to bring up backup interface
#define BACKUP_START_CMD "/sbin/ifup"
#define BACKUP_WWAN_START_CMD "/usr/bin/quectel-CM -s irisbylowes.com -f /tmp/quectel.log &"

// Commands to start/stop/init LTE and check primary connection
#define LTE_START_COMMAND "/usr/bin/backup_start"
#define LTE_STOP_COMMAND  "/usr/bin/backup_stop"
#define LTE_INIT_COMMAND  "/sbin/ifconfig eth1 up; /sbin/ifconfig eth1 down"
#define CHECK_PRIMARY_CMD "/usr/bin/check_primary"
