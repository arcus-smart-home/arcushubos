# Copyright (C) 2013-2016 Freescale Semiconductor

DESCRIPTION = "i.MX U-Boot suppporting i.MX reference boards."
require recipes-bsp/u-boot/u-boot.inc

PROVIDES += "u-boot"
DEPENDS_append = "dtc-native"

LICENSE = "GPLv2+"
LIC_FILES_CHKSUM = "file://Licenses/gpl-2.0.txt;md5=b234ee4d69f5fce4486a80fdaf4a4263"

SRCBRANCH = "nxp/imx_v2016.03_4.1.15_2.0.0_ga"
SRC_URI = "git://source.codeaurora.org/external/imx/uboot-imx.git;protocol=https;branch=${SRCBRANCH}"
SRCREV = "0ec2a019117bb2d59b9672a145b4684313d92782"

S = "${WORKDIR}/git"

inherit fsl-u-boot-localversion

LOCALVERSION ?= "-${SRCBRANCH}"

PACKAGE_ARCH = "${MACHINE_ARCH}"
COMPATIBLE_MACHINE = "(imxdimagic)"

SRC_URI += " \
	file://0001-imxdimagic-platform-support-2016.03.patch \
	file://0002-mx6imagic-IRIS-multi-partition-2016.03.patch \
	file://0003-REV-B-DDR-Support.patch \
	file://0004-Fix-to-Samsung-K4B4G1646E-BYMA-DDR3-1866.patch \
	file://0005-Lockdown-for-production.patch \
	file://0006-Update-to-Micron-DDR-settings.patch \
	"
