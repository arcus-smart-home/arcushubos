# Copyright (C) 2016 NXP Semiconductors
# Released under the MIT license (see COPYING.MIT for the terms)

include recipes-kernel/linux/linux-fslc.inc

DESCRIPTION = "Linux kernel based on linux-fsl-imx branch 4.1-1.0.x-imx from FSL Community BSP \
with additional patches to cover devices specific on WaRP7 board."

DEPENDS += "lzop-native bc-native"

SRCBRANCH = "4.1-1.0.x-imx"
SRCREV = "a543d1bd884028bed97c06ae4087e051a9176499"
LOCALVERSION = "-${SRCBRANCH}-warp7"

SRC_URI = "git://github.com/warp7/linux-fslc.git;branch=${SRCBRANCH} \
           file://defconfig"

COMPATIBLE_MACHINE = "(imx7s-warp)"
