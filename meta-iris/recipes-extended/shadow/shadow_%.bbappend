
# We only need a subset of the full shadow package support since
#  we have a read-only root filesystem and do not intend to allow
#  passwords to be changed
FILES_${PN} = "${base_bindir}/* \
	       ${base_sbindir}/nologin \
	       ${sysconfdir} \
	       ${sharedstatedir} \
	       ${localstatedir} \
	       ${bindir}/groups.shadow \
	       ${sbindir}/logoutd \
	       "

# Stop complaints about missing files...
INSANE_SKIP_${PN} = "installed-vs-shipped"