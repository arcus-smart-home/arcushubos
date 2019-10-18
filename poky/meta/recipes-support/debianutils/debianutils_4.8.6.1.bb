SUMMARY = "Miscellaneous utilities specific to Debian"
SECTION = "base"
LICENSE = "GPLv2 & SMAIL_GPL"
LIC_FILES_CHKSUM = "file://debian/copyright;md5=f01a5203d50512fc4830b4332b696a9f"

SRC_URI = "http://snapshot.debian.org/archive/debian/20190217T160716Z/pool/main/d/${BPN}/${BPN}_${PV}.tar.xz"
# the package is taken from snapshots.debian.org; that source is static and goes stale
# so we check the latest upstream from a directory that does get updated
UPSTREAM_CHECK_URI = "${DEBIAN_MIRROR}/main/d/${BPN}/"

SRC_URI[md5sum] = "80e2e670d8f6c0036770e971237f1f5c"
SRC_URI[sha256sum] = "099f1e8a7278b26145a2ba2dda84c4118403bfab38c8d7070a6235a7ffcb55ed"

S = "${WORKDIR}/${BPN}"

inherit autotools update-alternatives

do_configure_prepend() {
    sed -i -e 's:tempfile.1 which.1:which.1:g' ${S}/Makefile.am
}

do_install_append() {
    if [ "${base_bindir}" != "${bindir}" ]; then
        # Debian places some utils into ${base_bindir} as does busybox
        install -d ${D}${base_bindir}
        for app in run-parts tempfile; do
            mv ${D}${bindir}/$app ${D}${base_bindir}/$app
        done
    fi
}

# Note that we package the update-alternatives name.
#
PACKAGES =+ "${PN}-run-parts"
FILES_${PN}-run-parts = "${base_bindir}/run-parts.debianutils"

RDEPENDS_${PN} += "${PN}-run-parts"
RDEPENDS_${PN}_class-native = ""

ALTERNATIVE_PRIORITY="30"
ALTERNATIVE_${PN} = "add-shell installkernel remove-shell savelog tempfile which"

ALTERNATIVE_PRIORITY_${PN}-run-parts = "60"
ALTERNATIVE_${PN}-run-parts = "run-parts"

ALTERNATIVE_${PN}-doc = "which.1"
ALTERNATIVE_LINK_NAME[which.1] = "${mandir}/man1/which.1"

ALTERNATIVE_LINK_NAME[add-shell]="${sbindir}/add-shell"
ALTERNATIVE_LINK_NAME[installkernel]="${sbindir}/installkernel"
ALTERNATIVE_LINK_NAME[remove-shell]="${sbindir}/remove-shell"
ALTERNATIVE_LINK_NAME[run-parts]="${base_bindir}/run-parts"
ALTERNATIVE_LINK_NAME[savelog]="${bindir}/savelog"
ALTERNATIVE_LINK_NAME[tempfile]="${base_bindir}/tempfile"
ALTERNATIVE_LINK_NAME[which]="${bindir}/which"

BBCLASSEXTEND = "native"
