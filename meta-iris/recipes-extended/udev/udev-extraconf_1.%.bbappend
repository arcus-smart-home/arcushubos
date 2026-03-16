FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

# Use our configuration files
SRC_URI += "file://mount.ignorelist \
	    file://mount.sh \
	    file://network.sh \
	    "
