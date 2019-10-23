DESCRIPTION = "Netty tcnative library"
HOMEPAGE = "https://https://github.com/netty/netty-tcnative"

DEPENDS = "openssl apr"
RDEPENDS_${PN} = "openssl apr"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/Apache-2.0;md5=89aea4e17d99a7cacdbeed46a0096b10"
ALLOW_EMPTY_${PN} = "1"
PR = "r0"

INSANE_SKIP_${PN} = "ldflags"

# We use the 2.0.5.Final code in the agent
PV = "2.0.5.Final"
BRANCH ?= "master"
SRCREV = "49b2322bcf8810fd6255c0aa7f9c8f5f0e61c7c4"

SRC_URI = "git://github.com/netty/netty-tcnative.git;protocol=git;branch=${BRANCH}"

SRC_URI += "file://Makefile \
	   file://jni.h \
	   file://jni_md.h \
	   "

S = "${WORKDIR}/git/openssl-dynamic/src/main/c"

do_compile_prepend() {
    cp ${WORKDIR}/Makefile ${S}/
    cp ${WORKDIR}/jni.h ${S}/
    cp ${WORKDIR}/jni_md.h ${S}/
}

do_compile () {
    make -C ${S}
}

do_install () {
	install -d ${D}${libdir}
	install -m 0755 ${S}/libnetty-tcnative.so.1.0 ${D}${libdir}
	ln -s ./libnetty-tcnative.so.1.0 ${D}${libdir}/libnetty-tcnative.so
	ln -s ./libnetty-tcnative.so.1.0 ${D}${libdir}/libnetty-tcnative.so.1
}



