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
#include <irislib.h>

static void usage(char *name)
{
    fprintf(stderr, "\nusage: %s [options]\n"
            "  options:\n"
            "    -h              Print this message\n"
            "    -t              Truncate data (used for BLE provisioning)\n"
            "                    Perform a WiFi network scan\n"
            "\n", name);
}

static int firstSSID = 1;

static void printSSIDData(int truncate, char *ssid, char *mode,
                          char *security[], int sIndex, char *wepauth,
                          char *encryption, int channel, int signal, char *wps)
{
    char securityList[256] = { 0 };
    int i, j;
    char escapedSSID[65] = { 0 };

    // Skip if SSID is null or bogus - iwlist sometimes returns a
    //  SSID with "\x00..." that I don't see on other devices
    if ((ssid == NULL) || (strncmp(ssid, "\\x00", 4) == 0)) {
        return;
    }

    // Build list of security options
    for (i = 0; i < sIndex; i++) {
        strcat(securityList, security[i]);
        if (i != (sIndex - 1)) {
            strcat(securityList, ",");
        }
    }
    if (firstSSID) {
        firstSSID = 0;
    } else {
        fprintf(stdout, ",");
    }

    // Need to escape \ and " in SSID to avoid JSON issues
    for (i = 0, j = 0; i < strlen(ssid); i++) {
        if ((ssid[i] == '\\') || (ssid[i] == '\"')) {
            escapedSSID[j++] = '\\';
        }
        escapedSSID[j++] = ssid[i];
    }

    // For BLE data, we want a smaller list of items
    if (truncate) {
        // We also don't show a security list, just a single item
        //  WPA2 wins out if both are present, etc.
        if (strstr(securityList, "WPA2")) {
            strcpy(securityList, "WPA2-PSK");
        } else if (strstr(securityList, "WPA")) {
            strcpy(securityList, "WPA-PSK");
        } else if (strstr(securityList, "WEP")) {
            strcpy(securityList, "WEP");
        } else {
            strcpy(securityList, "None");
        }
        fprintf(stdout, "{\"ssid\":\"%s\", \"security\":\"%s\", "
                "\"channel\":%d, \"signal\":%d}", escapedSSID, securityList,
                channel, signal);
    } else {
        fprintf(stdout, "{\"ssid\":\"%s\", \"mode\":\"%s\", \"security\":[%s], "
                "\"wepauth\":\"%s\", \"encryption\":\"%s\", \"channel\":%d, "
                "\"signal\":%d, \"wps\":\"%s\"}", escapedSSID, mode,
                securityList, wepauth, encryption, channel, signal, wps);
    }
}


// Perform WiFi scan
int main(int argc, char** argv)
{
    int  c, truncate = 0;
    char cmd[256];
    FILE *f;

    // Parse options...
    opterr = 0;
    while ((c = getopt(argc, argv, "ht")) != -1)
    switch (c) {
    case 't':
        truncate = 1;
        break;
    case 'h':
        usage(argv[0]);
        return 0;
    case '?':
        fprintf(stderr, "Unknown option `\\x%x'.\n", optopt);
        usage(argv[0]);
        exit(1);
    default:
        fprintf(stderr, "Error parsing options - exiting!\n");
        usage(argv[0]);
        exit(1);
    }

    // Start data output
    fprintf(stdout, "{\"scanresults\":\n[");

    // Run scan command
    snprintf(cmd, sizeof(cmd), "/sbin/iwlist %s scan", WIFI_IF);
    f = popen(cmd, "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f) != NULL) {
        loop:
            if (strstr(line, "Cell ") != NULL) {
                char ssid[64] = { 0 };
                char bssid[32] = { 0 };
                char *mode = "N/A";
                char *security[10];
                int sIndex = 0;
                char *wepauth = "N/A";
                char *encryption = "N/A";
                int channel = 0;
                int signal = 0;
                char *ptr;
                // Not provided with iwlist output and I don't it will
                //  be needed as we won't support from hub
                char *wps = "No";

                // First line always has BSSID
                ptr = strstr(line, "Address:");
                if (ptr != NULL) {
                    ptr += strlen("Address: ");
                    strncpy(bssid, ptr, sizeof(bssid));
                }

                // Next line has channel info
                if (fgets(line, sizeof(line), f) == NULL) {
                    break;
                }
                ptr = strstr(line, "Channel:");
                if (ptr != NULL) {
                    ptr += strlen("Channel:");
                    channel = atoi(ptr);
                }

                // Skip frequency info
                if (fgets(line, sizeof(line), f) == NULL) {
                    break;
                }

                // Next line has signal info
                if (fgets(line, sizeof(line), f) == NULL) {
                    break;
                }
                ptr = strstr(line, "Quality=");
                if (ptr != NULL) {
                    char value1[3], value2[3];
                    ptr += strlen("Quality=");
                    value1[0] = *ptr++;
                    value1[1] = *ptr++;
                    value1[2] = '\0';
                    ptr++;
                    value2[0] = *ptr++;
                    value2[1] = *ptr++;
                    value2[2] = '\0';
                    signal = (atoi(value1) * 100) / atoi(value2);
                }

                // Next line has encryption info
                if (fgets(line, sizeof(line), f) == NULL) {
                    break;
                }
                if (strstr(line, "off") != NULL) {
                    // No security
                    security[sIndex++] = "\"None\"";
                    wepauth = "Unknown";
                    encryption = "None";
                }

                // Next line has SSID info
                if (fgets(line, sizeof(line), f) == NULL) {
                    break;
                }
                ptr = strstr(line, "ESSID:");
                if (ptr != NULL) {
                    int len;
                    // Skip quotes
                    ptr += strlen("ESSID:") + 1;
                    len = strlen(ptr) - 2;
                    strncpy(ssid, ptr, sizeof(ssid));
                    ssid[len] = '\0';
                }

                // Skip bit rates to get to mode
                while (fgets(line, sizeof(line), f) != NULL) {
                    if (strstr(line, "Mode:") != NULL) {
                        break;
                    }
                }
                if (strstr(line, "Master") != NULL) {
                    mode = "Infrastructure";
                } else {
                    mode = "Ad-hoc";
                }

                // Look for encryption data if we haven't found any
                if (sIndex == 0) {
                    // Skip to get to IE data
                    while (fgets(line, sizeof(line), f) != NULL) {
                        if (strstr(line, "Cell ") != NULL) {

                          // If we hit nothing, must be WEP
                          if (sIndex == 0) {
                              security[sIndex++] = "\"WEP\"";
                              // Don't believe we can actually tell,
                              //  shouldn't matter to client
                              wepauth = "OpenSystem";
                              encryption = "WEP";
                          }
                          // Dump SSID data
                          printSSIDData(truncate, ssid, mode, security, sIndex,
                                        wepauth, encryption, channel, signal,
                                        wps);
                          // Done!
                          goto loop;
                        } else if (strstr(line, "IE: ") != NULL) {
                            // Some form of WPA - don't worry about
                            //  enterprise support, Hub shouldn't support
                            //   this for normal case home users!
                            wepauth = "OpenSystem";

                            if (strstr(line, "WPA Version 1") != NULL) {
                                security[sIndex++] = "\"WPA-PSK\"";
                                fgets(line, sizeof(line), f);
                                if (strstr(line, "TPIK") != NULL) {
                                    encryption = "TPIK";
                                } else {
                                    encryption = "AES";
                                }
                            } else if (strstr(line, "WPA2") != NULL) {
                                security[sIndex++] = "\"WPA2-PSK\"";
                                encryption = "AES+TPIK";
                            }
                        }
                    }
                }
                // Dump SSID data
                printSSIDData(truncate, ssid, mode, security, sIndex, wepauth,
                              encryption, channel, signal, wps);
            }
        }
        pclose(f);
    }

    // End data output
    fprintf(stdout, "]}\n");
    return 0;
}
