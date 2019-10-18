DESCRIPTION = "The goal of this project is to provide a tiny library that would \
facilitate the common operations with sizes in bytes."
HOMEPAGE = "https://github.com/rhinstaller/libbytesize"
LICENSE = "LGPLv2+"
SECTION = "devel/lib"

LIC_FILES_CHKSUM = "file://LICENSE;md5=c07cb499d259452f324bb90c3067d85c"

S = "${WORKDIR}/git"
B = "${S}"

SRCREV = "ebbda2217d177d2a17f917d7a3e3d132a26db791"
SRC_URI = "git://github.com/rhinstaller/libbytesize;branch=master \
"

inherit gettext autotools python3native

DEPENDS += " \
    libpcre \
    gmp \
    mpfr \
"

FILES_${PN} += "${PYTHON_SITEPACKAGES_DIR}/bytesize"

PACKAGECONFIG ??= "python3"
PACKAGECONFIG[python3] = "--with-python3, --without-python3,,python3"
PACKAGECONFIG[python2] = "--with-python2, --without-python2,,python2"
PACKAGECONFIG[doc] = "--with-gtk-doc, --without-gtk-doc, gtk-doc-native"

EXTRA_OEMAKE = "py3libdir=${PYTHON_SITEPACKAGES_DIR}"


