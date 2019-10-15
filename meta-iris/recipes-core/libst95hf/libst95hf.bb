#
# ST Micro ST95 NFC Library support
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

DESCRIPTION = "ST Micro 95HF NFC library"

LICENSE = "ST-Micro"
LIC_FILES_CHKSUM = "file://${WORKDIR}/git/Release_Notes.html;md5=81e8fc6f0ac0024bf196725dfc3835d5"

PV = "1.0"
PR = "r0"

COMPATIBLE_MACHINE = "imxdimagic"

BRANCH ?= "master"
SRCREV = "ed8cabcc1e1736ec76cbb7b0bed064d06b4e7d26"

SRC_URI = "git://github.com/ngohaibac/ST95HF.git;protocol=git;branch=${BRANCH}"

SRC_URI += "file://Makefile \
	   file://libst95hf.h \
	   file://0001-IRIS-mods.patch \
	   file://0002-Fixed-issues-with-tag-handling.patch \
	   file://0003-Fix-tag-read-issues.patch \
	   "
S = "${WORKDIR}/git"

do_compile_prepend() {
    cp ${WORKDIR}/Makefile ${S}/
}

do_install () {
	install -d ${D}${includedir}
	install -m 0444 ${WORKDIR}/libst95hf.h ${D}${includedir}

	install -d ${D}${libdir}
	install -m 0755 libst95hf.so.1.0 ${D}${libdir}
	ln -s ./libst95hf.so.1.0 ${D}${libdir}/libst95hf.so
	ln -s ./libst95hf.so.1.0 ${D}${libdir}/libst95hf.so.1
}
