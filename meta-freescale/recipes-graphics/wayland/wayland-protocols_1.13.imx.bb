SUMMARY = "Collection of additional Wayland protocols"
DESCRIPTION = "Wayland protocols that add functionality not \
available in the Wayland core protocol. Such protocols either add \
completely new functionality, or extend the functionality of some other \
protocol either in Wayland core, or some other protocol in \
wayland-protocols."
HOMEPAGE = "http://wayland.freedesktop.org"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://COPYING;md5=c7b12b6702da38ca028ace54aae3d484 \
                    file://stable/presentation-time/presentation-time.xml;endline=26;md5=4646cd7d9edc9fa55db941f2d3a7dc53"

ARCHIVE_NAME = "${BPN}-1.13"
SRC_URI = "https://wayland.freedesktop.org/releases/${ARCHIVE_NAME}.tar.xz \
           file://0001-unstable-Add-alpha-compositing-protocol.patch \
           file://0002-unstable-Add-hdr10-metadata-protocol.patch"
SRC_URI[md5sum] = "29312149dafcd4a0e739ba94995a574d"
SRC_URI[sha256sum] = "0758bc8008d5332f431b2a84fea7de64d971ce270ed208206a098ff2ebc68f38"
S = "${WORKDIR}/${ARCHIVE_NAME}"

inherit autotools pkgconfig

PACKAGES = "${PN}"
FILES_${PN} += "${datadir}/pkgconfig/wayland-protocols.pc"

PACKAGE_ARCH = "${MACHINE_SOCARCH}"
COMPATIBLE_MACHINE = "(imxfbdev|imxgpu)"
