SUMMARY = "Library for reading extended image information (EXIF) from JPEG files"
HOMEPAGE = "http://sourceforge.net/projects/libexif"
SECTION = "libs"
LICENSE = "LGPLv2.1"
LIC_FILES_CHKSUM = "file://COPYING;md5=243b725d71bb5df4a1e5920b344b86ad"

SRC_URI = "${SOURCEFORGE_MIRROR}/libexif/libexif-${PV}.tar.bz2 \
           file://CVE-2017-7544.patch \
           file://CVE-2016-6328.patch \
           file://CVE-2018-20030.patch"

SRC_URI[md5sum] = "27339b89850f28c8f1c237f233e05b27"
SRC_URI[sha256sum] = "16cdaeb62eb3e6dfab2435f7d7bccd2f37438d21c5218ec4e58efa9157d4d41a"

inherit autotools gettext

EXTRA_OECONF += "--disable-docs"
