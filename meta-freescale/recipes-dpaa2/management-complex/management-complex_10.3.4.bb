SUMMARY = "DPAA2 Management Complex Firmware"
LICENSE = "Freescale-Binary-EULA"
LIC_FILES_CHKSUM = "file://LICENSE;md5=0ef9ba7621bd36f5fef5009fbeb56b00"

inherit deploy

INHIBIT_DEFAULT_DEPS = "1"

SRC_URI = "git://github.com/qoriq-open-source/mc-binary.git;nobranch=1"
SRCREV = "824745fe1580b281bebb808321c3f41353a3140a"

S = "${WORKDIR}/git"

REGLEX_ls2088a = "ls2088a"
REGLEX_ls1088a = "ls1088a"

do_install () {
    install -d ${D}/boot
    install -m 755 ${S}/${REGLEX}/*.itb ${D}/boot
}

do_deploy () {
    install -d ${DEPLOYDIR}/mc_app
    install -m 755 ${S}/${REGLEX}/*.itb ${DEPLOYDIR}/mc_app
    # make a symlink to the latest binary
    for mc_binary in `ls ${DEPLOYDIR}/mc_app |sort`;do
        ln -sfT ${mc_binary} ${DEPLOYDIR}/mc_app/mc.itb
    done
}
addtask deploy before do_build after do_install

PACKAGES += "${PN}-image"
FILES_${PN}-image += "/boot"

INHIBIT_PACKAGE_STRIP = "1"

COMPATIBLE_MACHINE = "(ls2080ardb|ls2088a|ls1088a)"
PACKAGE_ARCH = "${MACHINE_ARCH}"

