#
# NFC support
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

DESCRIPTON = "Iris NFC support"
DEPENDS = "libst95hf iris-lib"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/Apache-2.0;md5=89aea4e17d99a7cacdbeed46a0096b10"

PR = "r0"

SRC_URI = "file://nfc_test.c \
           file://irisnfcd.c \
           "

FILES_${PN} += "${libdir}"

# Consider any warnings errors (well, not ignored results)
CFLAGS += "-Werror -Wno-unused-result"

# Make sure we build position independent code
CFLAGS += "-fPIC"

# Add platform specific defines
TARGET_MACHINE := "${@'${MACHINE}'.replace('-', '_')}"
CFLAGS += "-D${TARGET_MACHINE}"

do_compile () {
	${CC} -g ${CFLAGS} ${LDFLAGS} ${WORKDIR}/nfc_test.c -o nfc_test -lst95hf
	${CC} -g ${CFLAGS} ${LDFLAGS} ${WORKDIR}/irisnfcd.c -o irisnfcd -lst95hf -liris
}

do_install () {
	install -d ${D}${bindir}
	install -m 0755 irisnfcd ${D}${bindir}
	install -m 0755 nfc_test ${D}${bindir}
}

