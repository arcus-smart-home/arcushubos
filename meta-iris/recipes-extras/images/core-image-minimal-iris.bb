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

CORE_IMAGE_EXTRA_INSTALL += " \
    dropbear \
    iris-utils \
    zigbee-utils \
    zwave-utils \
    ble-utils \
    e2fsprogs-e2fsck \
    e2fsprogs-mke2fs \
    e2fsprogs-tune2fs \
    openssl \
    openssl-bin \
    udev-extraconf \
    i2c-tools \
    sqlite3 \
    iris-agent \
    ca-certificates \
    tcpdump \
    tzdata \
    cryptodev-module \
    iris-4g \
    openjdk-8-armhf \
    "


SUMMARY = "A small image with IRIS hub support" 

IMAGE_INSTALL = "packagegroup-core-boot \
	      packagegroup-base packagegroup-base-extended \
	      packagegroup-distro-base packagegroup-machine-base \
	      packagegroup-base-usbhost ${CORE_IMAGE_EXTRA_INSTALL}"

IMAGE_LINGUAS = " "

# Remove some packages that we really don't want
PACKAGE_EXCLUDE_pn-core-image-minimal-iris = "rhino jamvm cacao \
                                           virtual/java-native \
                                           virtual/java-initial \
                                           "

EXTRA_IMAGE_FEATURES += "read-only-rootfs"

LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/Apache-2.0;md5=89aea4e17d99a7cacdbeed46a0096b10"

inherit core-image

# Set default password - generated with "openssl passwd -1"
ROOTFS_POSTPROCESS_COMMAND += "set_root_passwd;"
set_root_passwd() {
   sed 's%^root:[^:]*:%root:$1$kPV0Qb8o$qUfyuYjesQTASUz4WGm3i1:%' \
       < ${IMAGE_ROOTFS}/etc/shadow \
       > ${IMAGE_ROOTFS}/etc/shadow.new;
   mv ${IMAGE_ROOTFS}/etc/shadow.new ${IMAGE_ROOTFS}/etc/shadow ;
}

# Allow agent to reboot and do other commands
ROOTFS_POSTPROCESS_COMMAND += "set_reboot_permissions;"
set_reboot_permissions() {
   chmod 4755 ${IMAGE_ROOTFS}/sbin/halt.sysvinit ;

   # Sound and wifi support
   if [ "${MACHINE}" = "imxdimagic" ]; then
      chmod 4755 ${IMAGE_ROOTFS}/usr/bin/speaker-test ;
      chmod 4755 ${IMAGE_ROOTFS}/usr/bin/amixer ;
      chmod 4755 ${IMAGE_ROOTFS}/usr/bin/play ;

      # Needed for agent to run wifi scan
      chmod 4755 ${IMAGE_ROOTFS}/usr/sbin/iw ;
   fi
}

# Set up links, etc
ROOTFS_POSTPROCESS_COMMAND += "set_file_links;"
set_file_links() {

	# Replace with our fstab file
	rm -f ${IMAGE_ROOTFS}/etc/fstab
	ln -s /home/root/etc/fstab ${IMAGE_ROOTFS}/etc/fstab

	# Replace with our default profile file
	rm -f ${IMAGE_ROOTFS}/etc/profile
	ln -s /home/root/etc/profile ${IMAGE_ROOTFS}/etc/profile

	# The new users and groups are created before the do_install
	# step, so you are now free to make use of them:
	chown -R agent ${IMAGE_ROOTFS}/home/agent/
	chgrp -R agent ${IMAGE_ROOTFS}/home/agent/

	# Replace /etc/securetty with a link to /var/run/securetty
	rm -f ${IMAGE_ROOTFS}/etc/securetty
	ln -s /var/run/securetty ${IMAGE_ROOTFS}/etc/securetty

	# Replace /etc/hosts with a link to /var/run/hosts
	rm -f ${IMAGE_ROOTFS}/etc/hosts
	ln -s /var/run/hosts ${IMAGE_ROOTFS}/etc/hosts

	# Replace wireless configuration files with links
	if [ "${MACHINE}" != "beaglebone-yocto" ]; then
	   rm -f ${IMAGE_ROOTFS}/etc/network/interfaces
	   ln -s /home/root/etc/interfaces ${IMAGE_ROOTFS}/etc/network/interfaces
	   rm -f ${IMAGE_ROOTFS}/etc/wpa_supplicant.conf
	   ln -s /data/config/wpa_supplicant.conf ${IMAGE_ROOTFS}/etc/wpa_supplicant.conf
	fi

	# Replace sound config file with link
	if [ "${MACHINE}" = "imxdimagic" ]; then
	   rm -f ${IMAGE_ROOTFS}/etc/asound.conf
	   ln -s /home/root/etc/asound.conf ${IMAGE_ROOTFS}/etc/asound.conf
	fi
}

# We do not want to write a timestamp as we have moved this file to
#  /data/config/timestamp which doesn't exist when the rootfs is made
rootfs_update_timestamp () {
}
