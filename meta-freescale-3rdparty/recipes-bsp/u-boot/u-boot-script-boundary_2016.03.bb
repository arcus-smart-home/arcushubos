require recipes-bsp/u-boot/u-boot-boundary-common_${PV}.inc

DEPENDS = "u-boot-mkimage-native"

inherit deploy

BOOTSCRIPT ??= "${S}/board/boundary/nitrogen6x/6x_bootscript-yocto-3.14.txt"
BOOTSCRIPT_use-mainline-bsp ??= "${S}/board/boundary/nitrogen6x/6x_bootscript-mainline.txt"

UPGRADESCRIPT = "${S}/board/boundary/nitrogen6x/6x_upgrade.txt"

do_mkimage () {
    # allow deploy to use the ${MACHINE} name to simplify things
    if [ ! -d ${S}/board/boundary/${MACHINE} ]; then
        mkdir ${S}/board/boundary/${MACHINE}
    fi

    uboot-mkimage -A arm -O linux -T script -C none -a 0 -e 0 \
                  -n "boot script" -d ${BOOTSCRIPT} \
                  ${S}/board/boundary/${MACHINE}/6x_bootscript

    uboot-mkimage -A arm -O linux -T script -C none -a 0 -e 0 \
                  -n "upgrade script" -d ${UPGRADESCRIPT} \
                  ${S}/board/boundary/${MACHINE}/6x_upgrade
}

addtask mkimage after do_compile before do_install

do_compile[noexec] = "1"

do_install () {
    install -D -m 644 ${S}/board/boundary/${MACHINE}/6x_bootscript \
                      ${D}/6x_bootscript
    install -D -m 644 ${S}/board/boundary/${MACHINE}/6x_upgrade \
                      ${D}/6x_upgrade
}

do_deploy () {
    install -D -m 644 ${D}/6x_bootscript \
                      ${DEPLOYDIR}/6x_bootscript-${MACHINE}-${PV}-${PR}
    install -D -m 644 ${D}/6x_upgrade \
                      ${DEPLOYDIR}/6x_upgrade-${MACHINE}-${PV}-${PR}

    cd ${DEPLOYDIR}
    rm -f 6x_bootscript-${MACHINE} 6x_upgrade-${MACHINE}
    ln -sf 6x_bootscript-${MACHINE}-${PV}-${PR} 6x_bootscript-${MACHINE}
    ln -sf 6x_upgrade-${MACHINE}-${PV}-${PR} 6x_upgrade-${MACHINE}
}

addtask deploy after do_install before do_build

FILES_${PN} += "/"

COMPATIBLE_MACHINE = "(nitrogen6x|nitrogen6x-lite|nitrogen6sx|nitrogen7)"
