#
# Create a subset of BlueZ DBUS API support to support GATT
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

DESCRIPTON = "Iris GPL library programs"
LICENSE = "LGPLv2.1 & GPLv2+"
LIC_FILES_CHKSUM = "file://${WORKDIR}/COPYING.LIB;md5=fb504b67c50331fc78734fed90fb0e09"

DEPENDS = "dbus glib-2.0"
RDEPENDS_${PN} = "dbus"
PR = "r0"

SRC_URI = "file://bluez-dbus.h \
           file://client.c \
           file://COPYING.LIB \
           file://gatt-service.c \
           file://gdbus.h \
           file://mainloop.c \
           file://object.c \
           file://polkit.c \
           file://watch.c \
           "

# Consider any warnings errors
CFLAGS += "-Wall -Werror"

# Make sure we build position independent code
CFLAGS += "-fPIC"

# Need dbus related header files
CFLAGS += "-I${RECIPE_SYSROOT}/usr/lib/dbus-1.0/include/ -I${STAGING_INCDIR}/dbus-1.0/"

# Add in glib flags
CFLAGS += "`/usr/bin/pkg-config --cflags glib-2.0`"

do_compile () {
        ${CC} ${CFLAGS} ${LDFLAGS} -c ${WORKDIR}/client.c -o client.o
        ${CC} ${CFLAGS} ${LDFLAGS} -c ${WORKDIR}/mainloop.c -o mainloop.o
        ${CC} ${CFLAGS} ${LDFLAGS} -c ${WORKDIR}/object.c -o object.o
        ${CC} ${CFLAGS} ${LDFLAGS} -c ${WORKDIR}/polkit.c -o polkit.o
        ${CC} ${CFLAGS} ${LDFLAGS} -c ${WORKDIR}/watch.c -o watch.o
        ${CC} ${CFLAGS} ${LDFLAGS} -c ${WORKDIR}/gatt-service.c -o \
	      	gatt-service.o
	${CC} ${LDFLAGS} -shared -fPIC -Wl,-soname,libbluez-dbus.so.1 \
	      -o libbluez-dbus.so.1.0 client.o gatt-service.o mainloop.o \
	      object.o polkit.o watch.o
}

do_install () {
	install -d ${D}${includedir}
	install -m 0444 ${WORKDIR}/bluez-dbus.h ${D}${includedir}

	install -d ${D}${libdir}
	install -m 0755 libbluez-dbus.so.1.0 ${D}${libdir}
	ln -s ./libbluez-dbus.so.1.0 ${D}${libdir}/libbluez-dbus.so
	ln -s ./libbluez-dbus.so.1.0 ${D}${libdir}/libbluez-dbus.so.1
}

