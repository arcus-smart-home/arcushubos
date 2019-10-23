FILESEXTRAPATHS_prepend := "${THISDIR}/files:"

# Use our configuration files
SRC_URI += "file://init \
	    file://dropbearkey.service \
	    file://dropbear@.service \
	    "


