
# IRIS - add in java cacerts keystore
FILES:${PN} += "/usr/lib/jvm/java-8-openjdk/jre/lib/security/cacerts"

# Create Java keystore from installed CA certificates (target only)
do_install:append:class-target () {
    install -d ${D}/usr/lib/jvm/java-8-openjdk/jre/lib/security/
    for cert in $(find ${D}${datadir}/ca-certificates -type f -name '*.crt') ; do
        echo "Adding $cert to keystore"
        cp $cert /tmp/certfile
        /usr/bin/keytool -importcert -noprompt -trustcacerts -alias `basename $cert` -file /tmp/certfile -keystore ${D}/usr/lib/jvm/java-8-openjdk/jre/lib/security/cacerts -storepass 'changeit'
        rm /tmp/certfile
    done
}
