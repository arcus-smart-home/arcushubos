FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

# Use our default configuration file to enable debugging so we
#  can determine password failures
SRC_URI += "file://defconfig"

# Replace gnutls with openssl to avoid a lot of added code in build

PACKAGECONFIG ??= "openssl"

# Ensure wpa_passphrase is always installed (split into separate package in 2.10)
RDEPENDS:${PN} += "${PN}-passphrase"
