#
# Install wireless firmware files we need on the hub - a very small
#  subset of the available "linux-firmware" package!
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

SUMMARY = "Wireless firmware files for use with hub Linux kernel"
SECTION = "kernel"

SRCREV = "5f8ca0c1db6106a2d6d7e85eee778917ff03c3de"
PE = "1"
PV = "0.0+git${SRCPV}"

SRC_URI = "git://git.kernel.org/pub/scm/linux/kernel/git/firmware/linux-firmware.git \
	file://brcmfmac43362-sdio.txt \
	file://brcmfmac43362-sdio.bin \
	"

S = "${WORKDIR}/git"

LICENSE = "LICENCE.rtlwifi_firmware.txt"
LIC_FILES_CHKSUM = "\
    file://${S}/LICENCE.rtlwifi_firmware.txt;md5=00d06cfd3eddd5a2698948ead2ad54a5 \
    file://${S}/LICENCE.broadcom_bcm43xx;md5=3160c14df7228891b868060e1951dfbc \
"
LICENSE_PATH += "${S}"

CLEANBROKEN = "1"
INSANE_SKIP_${PN} = "installed-vs-shipped "

FILES_${PN}-license += "/lib/firmware/LICEN*rtl*"
FILES_${PN}-license += "/lib/firmware/LICENCE.broadcom_bcm43xx"
FILES_${PN} += "/lib/firmware/rtlwifi/*8192cu*"
FILES_${PN} += "/lib/firmware/rtlwifi/*8188*"
FILES_${PN} += "/lib/firmware/brcm/brcmfmac43362-sdio.bin"
FILES_${PN} += "/lib/firmware/brcm/brcmfmac43362-sdio.txt"

do_compile() {
	:
}

do_install() {
	install -d  ${D}${base_libdir}/firmware/

	# Just RTL (Realtek 8188/8192) support for now
	install -d  ${D}${base_libdir}/firmware/rtlwifi/
	cp -r ${S}/rtlwifi/*8192cu* ${D}${base_libdir}/firmware/rtlwifi/
	cp -r ${S}/rtlwifi/*8188* ${D}${base_libdir}/firmware/rtlwifi/
	cp ${S}/LICEN*rtl* ${D}${base_libdir}/firmware/

	# And Broadcom support for AP6181
	install -d  ${D}${base_libdir}/firmware/brcm/
	cp -r ${S}/brcm/*43362* ${D}${base_libdir}/firmware/brcm/
	# This NVRAM .txt file is not part of the current package,
	#  so include separately in this package for now to allow
	#  driver to come up
        cp -r ${WORKDIR}/brcmfmac43362-sdio.txt ${D}${base_libdir}/firmware/brcm/
        # Update to later firmware file as well (from Broadcom via GS)
        cp -r ${WORKDIR}/brcmfmac43362-sdio.bin ${D}${base_libdir}/firmware/brcm/
	cp ${S}/LICEN*bcm43xx* ${D}${base_libdir}/firmware/
}

