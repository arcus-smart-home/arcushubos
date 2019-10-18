SUMMARY = "Generic font configuration library"
DESCRIPTION = "Fontconfig is a font configuration and customization library, which \
does not depend on the X Window System. It is designed to locate \
fonts within the system and select them according to requirements \
specified by applications. \
Fontconfig is not a rasterization library, nor does it impose a \
particular rasterization library on the application. The X-specific \
library 'Xft' uses fontconfig along with freetype to specify and \
rasterize fonts."

HOMEPAGE = "http://www.fontconfig.org"
BUGTRACKER = "https://bugs.freedesktop.org/enter_bug.cgi?product=fontconfig"

LICENSE = "MIT-style & MIT & PD"
LIC_FILES_CHKSUM = "file://COPYING;md5=7a0449e9bc5370402a94c00204beca3d \
                    file://src/fcfreetype.c;endline=45;md5=5d9513e3196a1fbfdfa94051c09dfc84 \
                    file://src/fccache.c;beginline=1367;endline=1382;md5=0326cfeb4a7333dd4dd25fbbc4b9f27f"

SECTION = "libs"

DEPENDS = "expat freetype zlib gperf-native"

SRC_URI = "http://fontconfig.org/release/fontconfig-${PV}.tar.gz \
           file://revert-static-pkgconfig.patch \
           file://0001-src-fcxml.c-avoid-double-free-of-filename.patch \
           file://0001-src-fccache.c-Fix-define-for-HAVE_POSIX_FADVISE.patch \
           "

SRC_URI[md5sum] = "00e748c67fad11e7057a71ed385e8bdb"
SRC_URI[sha256sum] = "064b9ebf060c9e77011733ac9dc0e2ce92870b574cca2405e11f5353a683c334"

UPSTREAM_CHECK_REGEX = "fontconfig-(?P<pver>\d+\.\d+\.(?!9\d+)\d+)"

do_configure_prepend() {
    # work around https://bugs.freedesktop.org/show_bug.cgi?id=101280
    rm -f ${S}/src/fcobjshash.h ${S}/src/fcobjshash.gperf
}

do_install_append_class-target() {
    # duplicate fc-cache for postinstall script
    mkdir -p ${D}${libexecdir}
    ln ${D}${bindir}/fc-cache ${D}${libexecdir}/${MLPREFIX}fc-cache
}

do_install_append_class-nativesdk() {
    # duplicate fc-cache for postinstall script
    mkdir -p ${D}${libexecdir}
    ln ${D}${bindir}/fc-cache ${D}${libexecdir}/${MLPREFIX}fc-cache
}

PACKAGES =+ "fontconfig-utils"
FILES_${PN} =+ "${datadir}/xml/*"
FILES_fontconfig-utils = "${bindir}/* ${libexecdir}/*"

# Work around past breakage in debian.bbclass
RPROVIDES_fontconfig-utils = "libfontconfig-utils"
RREPLACES_fontconfig-utils = "libfontconfig-utils"
RCONFLICTS_fontconfig-utils = "libfontconfig-utils"
DEBIAN_NOAUTONAME_fontconfig-utils = "1"

inherit autotools pkgconfig relative_symlinks

FONTCONFIG_CACHE_DIR ?= "${localstatedir}/cache/fontconfig"

# comma separated list of additional directories
# /usr/share/fonts is already included by default (you can change it with --with-default-fonts)
FONTCONFIG_FONT_DIRS ?= "no"

EXTRA_OECONF = " --disable-docs --with-default-fonts=${datadir}/fonts --with-cache-dir=${FONTCONFIG_CACHE_DIR} --with-add-fonts=${FONTCONFIG_FONT_DIRS}"

BBCLASSEXTEND = "native nativesdk"
