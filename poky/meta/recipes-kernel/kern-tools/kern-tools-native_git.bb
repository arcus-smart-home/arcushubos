SUMMARY = "Tools for managing Yocto Project style branched kernels"
LICENSE = "GPLv2"
LIC_FILES_CHKSUM = "file://git/tools/kgit;beginline=5;endline=9;md5=a6c2fa8aef1bda400e2828845ba0d06c"

DEPENDS = "git-native"

SRCREV = "af1a779f662c81da521e4d602f3c6446547d12a2"
PR = "r12"
PV = "0.2+git${SRCPV}"

inherit native

SRC_URI = "git://git.yoctoproject.org/yocto-kernel-tools.git"
S = "${WORKDIR}"
UPSTREAM_CHECK_COMMITS = "1"

do_compile() { 
	:
}

do_install() {
	cd ${S}/git
	make DESTDIR=${D}${bindir} install
}
