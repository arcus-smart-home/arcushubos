SUMMARY = "OpenGL function pointer management library"
HOMEPAGE = "https://github.com/anholt/libepoxy/"
SECTION = "libs"

LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://COPYING;md5=58ef4c80d401e07bd9ee8b6b58cf464b"

SRC_URI = "https://github.com/anholt/${BPN}/releases/download/${PV}/${BP}.tar.xz \
           "
SRC_URI[md5sum] = "e2845de8d2782b2d31c01ae8d7cd4cbb"
SRC_URI[sha256sum] = "002958c5528321edd53440235d3c44e71b5b1e09b9177e8daf677450b6c4433d"
UPSTREAM_CHECK_URI = "https://github.com/anholt/libepoxy/releases"

inherit meson pkgconfig distro_features_check

REQUIRED_DISTRO_FEATURES = "opengl"
REQUIRED_DISTRO_FEATURES_class-native = ""
REQUIRED_DISTRO_FEATURES_class-nativesdk = ""

PACKAGECONFIG[egl] = "-Degl=yes, -Degl=no, virtual/egl"
PACKAGECONFIG[x11] = "-Dglx=yes, -Dglx=no, virtual/libx11 virtual/libgl"
PACKAGECONFIG ??= "${@bb.utils.filter('DISTRO_FEATURES', 'x11', d)} egl"

EXTRA_OEMESON += "-Dtests=false"

PACKAGECONFIG_class-native = "egl"
PACKAGECONFIG_class-nativesdk = "egl"

BBCLASSEXTEND = "native nativesdk"

# This will ensure that dlopen will attempt only GL libraries provided by host
do_install_append_class-native() {
	chrpath --delete ${D}${libdir}/*.so
}

do_install_append_class-nativesdk() {
	chrpath --delete ${D}${libdir}/*.so
}
