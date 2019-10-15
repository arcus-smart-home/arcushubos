#
# Create various Zigbee utility programs
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

DESCRIPTON = "Zigbee utility programs"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/Apache-2.0;md5=89aea4e17d99a7cacdbeed46a0096b10"

DEPENDS = "iris-lib"
RDEPENDS_${PN} = "iris-lib"
PR = "r1"

SRC_URI = "file://zigbee_fsl_flash \
	   file://zigbee_ti_flash \
	   file://zigbee_fsl_default \
	   file://zigbee_ti_default \
	   file://zigbee_fsl_mfg_tokens \
	   file://zigbee_ti_mfg_tokens \
	   "

# Avoid "was already stripped, this will prevent future debugging!" errors
INSANE_SKIP_${PN} = "already-stripped"

do_install () {
	install -d ${D}${bindir}
	if [ "${MACHINE}" = "imxdimagic" ]; then
	    install -m 4755 ${WORKDIR}/zigbee_fsl_flash ${D}${bindir}/zigbee_flash
	    install -m 4755 ${WORKDIR}/zigbee_fsl_default ${D}${bindir}/zigbee_default
	    install -m 4755 ${WORKDIR}/zigbee_fsl_mfg_tokens ${D}${bindir}/zigbee_mfg_tokens
	else
	    install -m 4755 ${WORKDIR}/zigbee_ti_flash ${D}${bindir}/zigbee_flash
	    install -m 4755 ${WORKDIR}/zigbee_ti_default ${D}${bindir}/zigbee_default
	    install -m 4755 ${WORKDIR}/zigbee_ti_mfg_tokens ${D}${bindir}/zigbee_mfg_tokens
	fi
}
