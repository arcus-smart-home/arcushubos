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
RDEPENDS_${PN} = "iris-lib iris-utils logrotate cronie"
PR = "r0"
S = "${WORKDIR}"

INITSCRIPT_NAME = "irisagent"
INITSCRIPT_PARAMS = "start 99 S ."

# the above init param sets up
# /etc/rcS.d# ls -ltr S99irisagent
# lrwxrwxrwx 1 root root 18 Feb 31  2000 S99irisagent -> ../init.d/irisagent

inherit autotools update-rc.d

# These need to be updated with each agent version change
AGENT_VERSION="2.13.26"
SRC_URI[md5sum] = "cefcf87c43a5fef4795f8119895bf3d9"
SRC_URI[sha256sum] = "2d16e871d6ba1d4c75ecb30f46e802bec2ec7dd3d9add446ce66b3f30570102d"
AGENT_FILE="iris-agent-hub-v2-${AGENT_VERSION}.tar.gz"

SRC_URI = "file://irisagent \
           file://irisagentd.c \
           file://0hourly \
           https://www.dropbox.com/s/4qtphxo52ckt3u3/${AGENT_FILE}?dl=1 \
           "
# Add to list if using local binary and remove server file https line
#	   file://iris-agent-hub

FILES_${PN} += "/home/agent \
               /home/root/.ssh \
               "

# Consider any warnings errors (well, not ignored results)
CFLAGS += "-Wall -Werror -Wno-unused-result"

# Add platform specific defines
TARGET_MACHINE := "${@'${MACHINE}'.replace('-', '_')}"
CFLAGS += "-D${TARGET_MACHINE}"

do_compile () {
        ${CC} ${CFLAGS} ${LDFLAGS} ${WORKDIR}/irisagentd.c -o irisagentd -liris
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
        # If a server exists with agent binaries, remove local file
        # and uncomment this next line (with appropriate login details
        # earlier in file
        install -m 0444 ${DL_DIR}/${AGENT_FILE}?dl=1 ${D}/home/agent/iris-agent-hub
        # For local binary, comment above line and uncomment below
        #install -m 0444 ${WORKDIR}/iris-agent-hub ${D}/home/agent/iris-agent-hub

        # Replace /home/root/.ssh/authorized_keys with a link to /var/run/..
        install -d ${D}/home/root/.ssh
        ln -s /var/run/authorized_keys ${D}/home/root/.ssh/authorized_keys
}

