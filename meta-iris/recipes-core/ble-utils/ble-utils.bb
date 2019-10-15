#
# Create BLE utility programs
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

DESCRIPTON = "BLE utility programs"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/Apache-2.0;md5=89aea4e17d99a7cacdbeed46a0096b10"

DEPENDS = "iris-lib tinycbor bluez-dbus glib-2.0"
RDEPENDS_${PN} = "iris-lib tinycbor bluez-dbus"
PR = "r0"

SRC_URI = "file://ble_prog.c \
           file://ble_ti_prog \
           file://ble_mcuboot_prog.c \
           file://base64.c \
           file://base64.h \
           file://wifi_prov.c \
           "

# Consider any warnings errors
CFLAGS += "-Wall -Werror"

CFLAGS += "-I${RECIPE_SYSROOT}/usr/lib/dbus-1.0/include/ -I${STAGING_INCDIR}/dbus-1.0/"

# Add platform specific defines
TARGET_MACHINE := "${@'${MACHINE}'.replace('-', '_')}"
CFLAGS += "-D${TARGET_MACHINE}"

# Avoid "was already stripped, this will prevent future debugging!" errors
INSANE_SKIP_${PN} = "already-stripped"

do_compile () {
    if [ "${MACHINE}" = "imxdimagic" ]; then
        ${CC} ${CFLAGS} ${LDFLAGS} ${WORKDIR}/base64.c ${WORKDIR}/ble_prog.c ${WORKDIR}/ble_mcuboot_prog.c -o ble_prog -liris -ltinycbor
        ${CC} ${CFLAGS} ${LDFLAGS} ${WORKDIR}/wifi_prov.c \
              -o wifi_prov -ldbus-1 -lbluez-dbus -liris -lpthread \
              `/usr/bin/pkg-config --cflags --libs glib-2.0`
    fi
}

do_install () {
	install -d ${D}${bindir}
	if [ "${MACHINE}" = "imxdimagic" ]; then
	   install -m 4755 ble_prog ${D}${bindir}
	   install -m 4755 wifi_prov ${D}${bindir}
	else
	   install -m 4755 ${WORKDIR}/ble_ti_prog ${D}${bindir}/ble_prog
	fi
}
