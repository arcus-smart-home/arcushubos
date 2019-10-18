#
# Create u-boot support for our hardware
#

#
# Copyright 2019 Arcus Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

require ${COREBASE}/meta/recipes-bsp/u-boot/u-boot.inc

FILESEXTRAPATHS_prepend := "${THISDIR}/files:"

LICENSE = "GPLv2+"
LIC_FILES_CHKSUM = "file://Licenses/README;md5=a2c678cfd4a4d97135585cad908541c6"

PACKAGE_ARCH = "${MACHINE_ARCH}"
COMPATIBLE_MACHINE = "beaglebone-yocto"

S = "${WORKDIR}/git"

PV = "2016.03"
PR = "r0"

# This is the 2016.03 u-boot release
SRCREV = "df61a74e6845ec9bdcdd48d2aff5e9c2c6debeaa"
SRC_URI = " \
    git://git.denx.de/u-boot.git;branch=master;protocol=git \
    file://0001-Iris-patches-2016.03.patch \
 "

UBOOT_LOCALVERSION = "-Iris-${PR}"
UBOOT_SUFFIX = "img"
SPL_BINARY = "MLO"
