
# IRIS - add in java cacerts keystore
FILES_${PN} += "/usr/lib/jvm/java-8-openjdk/jre/lib/security/cacerts"

# Need to create default keystore file for agent use
do_install () {
    install -d ${D}${datadir}/ca-certificates \
               ${D}${sysconfdir}/ssl/certs \
               ${D}${sysconfdir}/ca-certificates/update.d
    oe_runmake 'DESTDIR=${D}' install

    install -d ${D}${mandir}/man8
    install -m 0644 sbin/update-ca-certificates.8 ${D}${mandir}/man8/

    install -d ${D}${sysconfdir}
    {
        echo "# Lines starting with # will be ignored"
        echo "# Lines starting with ! will remove certificate on next update"
        echo "#"
        find ${D}${datadir}/ca-certificates -type f -name '*.crt' | \
            sed 's,^${D}${datadir}/ca-certificates/,,'
    } >${D}${sysconfdir}/ca-certificates.conf

    # IRIS - create default keystore file
    install -d ${D}/usr/lib/jvm/java-8-openjdk/jre/lib/security/
    for cert in $(find ${D}${datadir}/ca-certificates -type f -name '*.crt') ; do
        echo "Adding $cert to keystore"
        cp $cert /tmp/certfile
        /usr/bin/keytool -importcert -noprompt -trustcacerts -alias `basename $cert` -file /tmp/certfile -keystore ${D}/usr/lib/jvm/java-8-openjdk/jre/lib/security/cacerts -storepass 'changeit'
        rm /tmp/certfile
    done
}
