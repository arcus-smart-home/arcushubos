#
# Create IRIS hub agent bootstrap support
#

#
# Copyright 2019 Arcus Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

DESCRIPTON = "Iris Hub Agent bootstrap"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/Apache-2.0;md5=89aea4e17d99a7cacdbeed46a0096b10"

DEPENDS = "iris-lib"
RDEPENDS:${PN} = "iris-lib iris-utils logrotate cronie"
PR = "r0"
S = "${WORKDIR}"

INITSCRIPT_NAME = "irisagent"
INITSCRIPT_PARAMS = "start 99 S ."

# the above init param sets up
# /etc/rcS.d# ls -ltr S99irisagent
# lrwxrwxrwx 1 root root 18 Feb 31  2000 S99irisagent -> ../init.d/irisagent

inherit autotools update-rc.d

# These need to be updated with each agent version change
AGENT_VERSION="2.13.26-prod"
SRC_URI[sha256sum] = "66685ff6998632cd035fcdfc692aad1a732fa13a5847e3c72f530a264bf3b7a7"
AGENT_FILE="iris-agent-hub-v2-${AGENT_VERSION}.tar.gz"

SRC_URI = "file://irisagent \
           file://irisagentd.c \
           file://0hourly \
           https://tools.arcus.wl-net.net/${AGENT_FILE}?dl=1 \
           "
# Add to list if using local binary and remove server file https line
#	   file://iris-agent-hub

FILES:${PN} += "/home/agent \
               /home/root/.ssh \
               "

# Consider any warnings errors (well, not ignored results)
CFLAGS += "-Wall -Werror -Wno-unused-result"

# Add platform specific defines
TARGET_MACHINE := "${@'${MACHINE}'.replace('-', '_')}"
CFLAGS += "-D${TARGET_MACHINE}"

# Allow the platform gateway URI to be overridden in local.conf
# e.g. IRIS_GATEWAY_URI = "wss://bh.arcussmarthome.com/hub/1.0"
IRIS_GATEWAY_URI ?= ""

do_compile[vardeps] += "IRIS_GATEWAY_URI"
do_compile () {
        if [ -n "${IRIS_GATEWAY_URI}" ]; then
                printf '#define IRIS_GATEWAY_URI "%s"\n' '${IRIS_GATEWAY_URI}' > ${WORKDIR}/gateway_uri.h
                GATEWAY_FLAG="-include ${WORKDIR}/gateway_uri.h"
        else
                GATEWAY_FLAG=""
        fi
        ${CC} ${CFLAGS} ${GATEWAY_FLAG} ${LDFLAGS} ${WORKDIR}/irisagentd.c -o irisagentd -liris
}

do_install () {
        install -d ${D}${bindir}
        install -m 0755 irisagentd ${D}${bindir}
        install -d ${D}${sysconfdir}/init.d
        install -m 0755 ${WORKDIR}/irisagent  ${D}${sysconfdir}/init.d/

        # Install cron setup needed for agent log maintenance
        # NO LONGER NEEDED AS WE SET UP HOURLY CRON CONFIG IN IRIS_UTILS!
        # install -d ${D}${sysconfdir}/cron.d
        # install -m 0444 ${WORKDIR}/0hourly  ${D}${sysconfdir}/cron.d/

        # Install hub agent tar file
        install -d ${D}/home/agent
        install -m 0444 ${DL_DIR}/${AGENT_FILE}?dl=1 ${D}/home/agent/iris-agent-hub

        # Replace /home/root/.ssh/authorized_keys with a link to /var/run/..
        install -d ${D}/home/root/.ssh
        ln -s /var/run/authorized_keys ${D}/home/root/.ssh/authorized_keys
}

