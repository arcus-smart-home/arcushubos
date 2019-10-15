SUMMARY = "Script-directed dynamic tracing and performance analysis tool for Linux"
HOMEPAGE = "https://sourceware.org/systemtap/"

require systemtap_git.inc

DEPENDS = "boost elfutils"

RDEPENDS_${PN} += "python3-core bash perl"

EXTRA_OECONF += "--with-libelf=${STAGING_DIR_TARGET} --without-rpm \
            --without-nss --without-avahi --without-dyninst \
            --disable-server --disable-grapher --enable-prologues \
            --with-python3 --without-python2-probes \
            ac_cv_prog_have_javac=no \
            ac_cv_prog_have_jar=no "

STAP_DOCS ?= "--disable-docs --disable-publican --disable-refdocs"

EXTRA_OECONF += "${STAP_DOCS} "

PACKAGECONFIG ??= "sqlite monitor python3-probes"
PACKAGECONFIG[libvirt] = "--enable-libvirt,--disable-libvirt,libvirt"
PACKAGECONFIG[sqlite] = "--enable-sqlite,--disable-sqlite,sqlite3"
PACKAGECONFIG[monitor] = "--enable-monitor,--disable-monitor,ncurses json-c"
PACKAGECONFIG[python3-probes] = "--with-python3-probes,--without-python3-probes,python3-setuptools-native"

inherit autotools gettext pkgconfig distutils3-base

BBCLASSEXTEND = "nativesdk"
