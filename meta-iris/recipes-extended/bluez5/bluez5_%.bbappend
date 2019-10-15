FILESEXTRAPATHS_prepend := "${THISDIR}/files:"

# Use our default init file and patches
SRC_URI += " \
	file://bluetooth.init \
	file://0001-Allow-GATT-access.patch \
	"

# Strip down package, in particular remove readline due to GPLv3 issues!
PACKAGECONFIG = "\
    ${@bb.utils.filter('DISTRO_FEATURES', 'systemd', d)} \
    tools \
    deprecated \
"

# Add some experimental features for server support
EXTRA_OECONF += "\
  --enable-experimental \
"

# Replace init file
do_compile_prepend() {
    cp ${WORKDIR}/bluetooth.init ${WORKDIR}/init
}

