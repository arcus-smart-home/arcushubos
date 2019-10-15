SUMMARY = "Timezone data"
HOMEPAGE = "http://www.iana.org/time-zones"
SECTION = "base"
LICENSE = "PD & BSD & BSD-3-Clause"
LIC_FILES_CHKSUM = "file://LICENSE;md5=ef1a352b901ee7b75a75df8171d6aca7"

DEPENDS = "tzcode-native"

SRC_URI = "http://www.iana.org/time-zones/repository/releases/tzdata${PV}.tar.gz;name=tzdata"

SRC_URI[tzdata.md5sum] = "73912ecfa6a9a8048ddf2e719d9bc39d"
SRC_URI[tzdata.sha256sum] = "b6966ec982ef64fe48cebec437096b4f57f4287519ed32dde59c86d3a1853845"

inherit allarch

S = "${WORKDIR}"

TZONES= "northamerica pacificnew systemv etcetera"

# Avoid complaints about uninstalled files that we do not need
INSANE_SKIP_${PN} = "installed-vs-shipped"

do_compile () {
        for zone in ${TZONES}; do \
            ${STAGING_BINDIR_NATIVE}/zic -d ${WORKDIR}${datadir}/zoneinfo -L /dev/null \
                -y ${S}/yearistype.sh ${S}/${zone} ; \
            ${STAGING_BINDIR_NATIVE}/zic -d ${WORKDIR}${datadir}/zoneinfo/posix -L /dev/null \
                -y ${S}/yearistype.sh ${S}/${zone} ; \
            ${STAGING_BINDIR_NATIVE}/zic -d ${WORKDIR}${datadir}/zoneinfo/right -L ${S}/leapseconds \
                -y ${S}/yearistype.sh ${S}/${zone} ; \
        done
}

do_install () {
        install -d ${D}/$exec_prefix ${D}${datadir}/zoneinfo
        cp -pPR ${S}/$exec_prefix ${D}/
        # libc is removing zoneinfo files from package
        cp -pP "${S}/zone.tab" ${D}${datadir}/zoneinfo
        cp -pP "${S}/iso3166.tab" ${D}${datadir}/zoneinfo
	# Create links to config directory so we can setup at boot
	install -d ${D}${sysconfdir}
	ln -s /data/config/localtime ${D}${sysconfdir}/localtime
	ln -s /data/config/timezone ${D}${sysconfdir}/timezone
        chown -R root:root ${D}
}

# Only include files we care about
FILES_${PN} += "${datadir}/zoneinfo/Pacific/* \
                ${datadir}/zoneinfo/US/* \
                ${datadir}/zoneinfo/Atlantic/* \
                ${datadir}/zoneinfo/America/* \
                ${datadir}/zoneinfo/Etc/* \
                ${datadir}/zoneinfo/CST6CDT \
                ${datadir}/zoneinfo/EST \
                ${datadir}/zoneinfo/EST5EDT \
                ${datadir}/zoneinfo/GMT \
                ${datadir}/zoneinfo/HST \
                ${datadir}/zoneinfo/MST \
                ${datadir}/zoneinfo/MST7MDT \
                ${datadir}/zoneinfo/PST8PDT \
                ${datadir}/zoneinfo/zone.tab \
                ${datadir}/zoneinfo/iso3166.tab \
		"     
