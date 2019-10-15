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

#include "irisversion.h"
#include "irisdefs.h"

//
//  Iris Library prototypes
//

// Is current image a release image?
int IRIS_isReleaseImage(void);

// Is current image a manufacturing image?
int IRIS_isMfgImage(void);

// Is current image a development image?
int IRIS_isDevImage(void);

// Is the SSH USB dongle debug key active?
int IRIS_isSSHEnabled(void);

// Are we still generating a SSH host key (at boot time)?
int IRIS_isSSHKeyGen(void);

// Return current hardware version
int IRIS_getHardwareVersion(void);

// Return current firmware version
int IRIS_getFirmwareVersion(char *version);

// Get hardware model
int IRIS_getModel(char *model);

// Get HubID
int IRIS_getHubID(char *hubID);

// Get MAC Addresses
int IRIS_getMACAddr1(char *macAddr);
int IRIS_getMACAddr2(char *macAddr);

// Does the hardware support flow control on UARTS 2 and 4?
int IRIS_isHwFlowControlSupported(void);

// Is battery voltage detection supported by the hardware?
int IRIS_isBatteryVoltageDetectionSupported(void);

// Is LTE modem connected?
int IRIS_isLTEConnected(void);

// Set LEDs to desired mode
void IRIS_setLedMode(char *mode);

// Get current LED mode setting
void IRIS_getLedMode(char *mode, size_t size);

// Initialize serial port to specific baud / flow-control mode
void IRIS_initSerialPort(int fd, int baud, int mode);

// Copy a local file
int IRIS_copyFile(const char *src, const char *dst);

// Play sound file
void IRIS_playAudio(char *filename, int delay);

// Check interface state
int IRIS_isIntfIPUp(char *intf);
int IRIS_isIntfConnected(char *intf);

// Power down hub, if possible
void IRIS_powerDownHub(void);

// Watchdog support
void startWatchdog(void);
void pokeWatchdog(void);
void stopWatchdog(void);
