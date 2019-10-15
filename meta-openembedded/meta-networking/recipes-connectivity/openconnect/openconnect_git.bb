SUMMARY = "Open client for Cisco AnyConnect VPN"
LICENSE = "LGPLv2.1"
LIC_FILES_CHKSUM = "file://COPYING.LGPL;md5=243b725d71bb5df4a1e5920b344b86ad"

DEPENDS = "vpnc libxml2 krb5 gettext-native"

PACKAGECONFIG ??= "gnutls lz4 libproxy"

# config defaults
PACKAGECONFIG[gnutls]    = "--with-gnutls,--without-gnutls,gnutls,"
PACKAGECONFIG[lz4]       = "--with-lz4,--without-lz4,lz4,"
PACKAGECONFIG[libproxy]  = "--with-libproxy,--without-libproxy,libproxy,"

# not config defaults
PACKAGECONFIG[pcsc-lite] = "--with-libpcsclite,--without-libpcsclite,pcsc-lite,"

PV = "7.08"

SRCREV = "9697bea0e50952cab57063238b43a7099e498cbb"
SRC_URI = "git://git.infradead.org/users/dwmw2/openconnect.git"

S = "${WORKDIR}/git"

inherit autotools pkgconfig

EXTRA_OECONF += "--with-vpnc-script=${SYSROOT_DESTDIR}${sysconfdir}/vpnc/vpnc-script \
                 --disable-static"
