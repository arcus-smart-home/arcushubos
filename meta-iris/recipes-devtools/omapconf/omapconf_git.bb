DESCRIPTION = "Diagnostic tool for TI OMAP processors"
HOMEPAGE = "https://github.com/omapconf/omapconf"

LICENSE = "GPLv2 | BSD"
LIC_FILES_CHKSUM = "file://LICENSE;md5=205c83c4e2242a765acb923fc766e914"

PV = "1.72+"
PR = "r0"

COMPATIBLE_MACHINE = "ti33x|ti43x|omap-a15|omap4|beaglebone-yocto"

BRANCH ?= "master"
SRCREV = "340f7dddb23664d8323c8d2002443606b95d969b"

SRC_URI = "git://github.com/omapconf/omapconf.git;protocol=git;branch=${BRANCH}"

S = "${WORKDIR}/git"

do_compile () {
    oe_runmake CROSS_COMPILE=${TARGET_PREFIX} install
}

do_install () {
    oe_runmake DESTDIR=${D}${bindir} install
}
