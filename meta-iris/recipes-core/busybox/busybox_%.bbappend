FILESEXTRAPATHS_prepend := "${THISDIR}/files:"

# Use our busybox default configuration file and syslog startup file
SRC_URI += "file://defconfig \
            file://syslog-startup.conf \
            file://simple.script \
            file://0001-set-killall-suid.patch \
            "
