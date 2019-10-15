#
# Support for 4G modem
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

DESCRIPTON = "Iris 4G modem support"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/Apache-2.0;md5=89aea4e17d99a7cacdbeed46a0096b10"

DEPENDS = "iris-lib"
RDEPENDS_${PN} = "iris-lib usb-modeswitch curl"
PR = "r1"
S = "${WORKDIR}"

INITSCRIPT_NAME = "iris4g"
INITSCRIPT_PARAMS = "start 99 S ."

# the above init param sets up
# /etc/rcS.d# ls -ltr S99iris4g
# lrwxrwxrwx 1 root root 18 Feb 31  2000 S99iris4g -> ../init.d/iris4g

inherit autotools update-rc.d

SRC_URI = "file://backup_start.c \
	   file://backup_stop.c \
	   file://check_primary.c \
	   file://backup.h \
	   file://usb-rule.rules \
	   file://usb_lte.sh \
	   file://iris4g \
	   file://iris4gd.c \
	   "

# Add platform specific defines
TARGET_MACHINE := "${@'${MACHINE}'.replace('-', '_')}"
CFLAGS += "-D${TARGET_MACHINE}"

do_compile() {
        ${CC} ${CFLAGS} ${LDFLAGS} ${WORKDIR}/backup_start.c -o backup_start -liris
        ${CC} ${CFLAGS} ${LDFLAGS} ${WORKDIR}/backup_stop.c -o backup_stop -liris
        ${CC} ${CFLAGS} ${LDFLAGS} ${WORKDIR}/check_primary.c -o check_primary -liris
        ${CC} ${CFLAGS} ${LDFLAGS} ${WORKDIR}/iris4gd.c -o iris4gd -lpthread -liris
}

do_install () {

	install -d ${D}${bindir}
	# These need to have suid set so agent can use them
	install -m 4755 backup_start ${D}${bindir}
	install -m 4755 backup_stop ${D}${bindir}
	install -m 4755 check_primary ${D}${bindir}

        install -m 0755 iris4gd ${D}${bindir}
        install -d ${D}${sysconfdir}/init.d
        install -m 0755 ${WORKDIR}/iris4g  ${D}${sysconfdir}/init.d/

	install -d ${D}${sysconfdir}/udev
	install -d ${D}${sysconfdir}/udev/rules.d
	install -m 0755 ${WORKDIR}/usb-rule.rules  ${D}${sysconfdir}/udev/rules.d/85-usb-rule.rules
	install -d ${D}${sysconfdir}/udev/scripts
	install -m 0755 ${WORKDIR}/usb_lte.sh  ${D}${sysconfdir}/udev/scripts/
}
