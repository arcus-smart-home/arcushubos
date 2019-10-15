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

enum ATState {IDLE = 0, DATA = 1};

enum ATStatus {OK = 0, ERROR = 1};

#define AT_DATA_BUF_SIZE 512
#define DEFAULT_AT_TIMEOUT 2
#define CONNECT_AT_TIMEOUT 15

typedef struct {
    int      fd;
    int      state;
    int      status;
    char     data[AT_DATA_BUF_SIZE];
} ATPort;

/* Open/close port */
ATPort *atOpen(char *port, int speed, int flow);
void atClose(ATPort *port);

/* AT send */
int atSend(ATPort *port, char *cmd, int timeout);

