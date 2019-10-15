SUMMARY = "Set of i2c tools for linux - just tools build, no perl needed"
SECTION = "base"
LICENSE = "GPLv2"
LIC_FILES_CHKSUM = "file://COPYING;md5=751419260aa954499f7abaabaa882bbe"

SRC_URI = "http://downloads.yoctoproject.org/mirror/sources/${BP}.tar.bz2 \
           file://Makefile \
"
SRC_URI[md5sum] = "7104a1043d11a5e2c7b131614eb1b962"
SRC_URI[sha256sum] = "db5e69f2e2a6e3aa2ecdfe6a5f490b149c504468770f58921c8c5b8a7860a441"

inherit autotools-brokensep

do_compile_prepend() {
    cp ${WORKDIR}/Makefile ${S}/
}

do_install_append() {
    install -d ${D}${includedir}/linux
    install -m 0644 include/linux/i2c-dev.h ${D}${includedir}/linux/i2c-dev-user.h
    rm -f ${D}${includedir}/linux/i2c-dev.h
}
