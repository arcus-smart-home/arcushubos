#
# Intel Concise Binary Object Representation (CBOR) Library
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

DESCRIPTION = "Intel CBOR library"

LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${WORKDIR}/git/LICENSE;md5=6c1ac30774dd6476b42b5b020cd2aa5f"

PV = "0.5.1"
PR = "r0"

BRANCH ?= "master"
SRCREV = "ae64a3d9da39f3bf310b9a7b38427c096d8bcd43"

SRC_URI = "git://github.com/intel/tinycbor.git;protocol=git;branch=${BRANCH}"

S = "${WORKDIR}/git"

INSANE_SKIP_${PN} = "ldflags"

do_install () {
	install -d ${D}${includedir}
	install -m 0444 ${S}/src/cbor.h ${D}${includedir}
	install -m 0444 ${S}/src/tinycbor-version.h ${D}${includedir}	

	install -d ${D}${libdir}
	install -m 0755 ${S}/lib/libtinycbor.so.0.5.1 ${D}${libdir}
	ln -s libtinycbor.so.0.5.1 ${D}${libdir}/libtinycbor.so
	ln -s libtinycbor.so.0.5.1 ${D}${libdir}/libtinycbor.so.0.5
}
