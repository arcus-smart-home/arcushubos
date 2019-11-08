#
# Create various IRIS library programs
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

DESCRIPTON = "Iris library programs"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/Apache-2.0;md5=89aea4e17d99a7cacdbeed46a0096b10"

PR = "r0"
DEPENDS_append_class-target = " iris-lib-native"
BBCLASSEXTEND += "native"

SRC_URI = "file://irisversion.h \
           file://irisdefs.h \
           file://irislib.h \
           file://irislib.c \
           file://aes.h \
           file://aes.c \
           file://at_parser.c \
           file://at_parser.h \
           "

# Consider any warnings errors (well, not ignored results)
CFLAGS += "-Wall -Werror -Wno-unused-result"

# Make sure we build position independent code
CFLAGS += "-fPIC"

# Add platform specific defines
TARGET_MACHINE := "${@'${MACHINE}'.replace('-', '_')}"
CFLAGS += "-D${TARGET_MACHINE}"

do_compile () {
        ${CC} ${CFLAGS} ${LDFLAGS} -c ${WORKDIR}/irislib.c -o irislib.o -lpthread
        ${CC} ${CFLAGS} ${LDFLAGS} -c ${WORKDIR}/aes.c -o aes.o
        ${CC} ${CFLAGS} ${LDFLAGS} -c ${WORKDIR}/at_parser.c -o at_parser.o
        ${CC} ${LDFLAGS} -shared -fPIC -Wl,-soname,libiris.so.1 -o libiris.so.1.0 irislib.o aes.o at_parser.o -lpthread
}

# Only headers are used natively
do_compile_class-native() {
}

do_install () {
	install -d ${D}${includedir}
	install -m 0444 ${WORKDIR}/irisversion.h ${D}${includedir}
	install -m 0444 ${WORKDIR}/irisdefs.h ${D}${includedir}
	install -m 0444 ${WORKDIR}/irislib.h ${D}${includedir}
	install -m 0444 ${WORKDIR}/aes.h ${D}${includedir}
	install -m 0444 ${WORKDIR}/at_parser.h ${D}${includedir}

	install -d ${D}${libdir}
	install -m 0755 libiris.so.1.0 ${D}${libdir}
	ln -s ./libiris.so.1.0 ${D}${libdir}/libiris.so
	ln -s ./libiris.so.1.0 ${D}${libdir}/libiris.so.1
}

do_install_class-native() {
	install -d ${D}${includedir}
	install -m 0444 ${WORKDIR}/irisversion.h ${D}${includedir}
	install -m 0444 ${WORKDIR}/irisdefs.h ${D}${includedir}
	install -m 0444 ${WORKDIR}/irislib.h ${D}${includedir}
}
