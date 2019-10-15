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

/* Utilities for daemons */

#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

static void daemon_sig_handler(int sig)
{
    switch (sig) {

    case SIGHUP:
        /* Ignore SIGHUP */
        break;
    case SIGTERM:
        /* Exit on SIGTERM */
        exit(0);
        break;
    }
}

void make_daemon(void)
{
    int i;

    /* Are we already a daemon? */
    if (getppid() == 1) {
        return;
    }

    /* Fork process */
    i = fork();

    /* Exit on error */
    if (i < 0) {
        exit(1);
    } else if (i > 0) {
        exit(0);  /* Parent exits without error */
    }

    /* Child continues from here ... */
    setsid();

    /* Close all descriptors */
    for (i = getdtablesize(); i >= 0;  --i) {
        close(i);
    }

    /* Handle standard I/O */
    i = open("/dev/null", O_RDWR);
    dup(i);
    dup(i);

    /* Use /tmp as work directory since read-only file system */
    umask(027);
    chdir("/tmp");

    /* Ignore some signals */
    signal(SIGCHLD, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);

    /* Catch others */
    signal(SIGHUP, daemon_sig_handler);
    signal(SIGTERM, daemon_sig_handler);
}
