SUMMARY = "Templatized C++ Command Line Parser"
HOMEPAGE = "http://tclap.sourceforge.net/"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://COPYING;md5=0ca8b9c5c5445cfa7af7e78fd27e60ed"

SRCREV = "75f440bcac1276c847f5351e14216f6e91def44d"
SRC_URI = "git://git.code.sf.net/p/tclap/code \
    file://Makefile.am-disable-docs.patch \
"

S = "${WORKDIR}/git"
inherit autotools

ALLOW_EMPTY_${PN} = "1"

BBCLASSEXTEND = "native nativesdk"
