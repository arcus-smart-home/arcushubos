FILESEXTRAPATHS_prepend := "${THISDIR}/files:"

# Use our default configuration file
SRC_URI += "file://logrotate.conf \
            file://logrotate.cron \
	    "

# We want to run logrotate hourly and have our own config file to add in
#  additional logs
do_install () {
    oe_runmake install DESTDIR=${D} PREFIX=${D} MANDIR=${mandir}
    mkdir -p ${D}${sysconfdir}/logrotate.d
    mkdir -p ${D}${sysconfdir}/cron.hourly
    mkdir -p ${D}${localstatedir}/lib
    touch ${D}${localstatedir}/lib/logrotate.status

    # Add in our log rotate configuration
    install -m 0644 ${WORKDIR}/logrotate.conf ${D}${sysconfdir}/

    # Add in our log rotate cron entry
    install -p -m 755 ${WORKDIR}/logrotate.cron ${D}${sysconfdir}/cron.hourly/logrotate
}


