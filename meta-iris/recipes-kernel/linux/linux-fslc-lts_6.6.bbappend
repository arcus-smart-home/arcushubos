FILESEXTRAPATHS:append := "${THISDIR}/linux-fslc-lts"

# Override KBUILD_DEFCONFIG from linux-fslc-lts_6.6.bb so our SRC_URI defconfig is used
KBUILD_DEFCONFIG:imxdimagic = ""

# lzop needed for CONFIG_KERNEL_LZO
DEPENDS += "lzop-native"

KERNEL_DEVICETREE = " \
    nxp/imx/imx6dl-imagic.dtb \
    nxp/imx/imx6q-imagic.dtb \
"

SRC_URI += " \
    file://defconfig \
    file://0001-IMAGIC-Hub-V3-device-tree-support.patch \
    file://0002-IMAGIC-Hub-V3-custom-driver-support.patch \
    file://0003-IMAGIC-Hub-V3-sound-machine-driver.patch \
    file://0004-FEC-support-fec_mac-boot-parameter.patch \
    file://0005-NAU8810-fix-PLL-mode-BCLK-divider-calculation.patch \
"
