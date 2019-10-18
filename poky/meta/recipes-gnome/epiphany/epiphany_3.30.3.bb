SUMMARY = "WebKit based web browser for GNOME"
LICENSE = "GPLv3+"
LIC_FILES_CHKSUM = "file://COPYING;md5=d32239bcb673463ab874e80d47fae504"

DEPENDS = "libsoup-2.4 webkitgtk gtk+3 iso-codes avahi libnotify gcr \
	   gsettings-desktop-schemas libxml2-native \
	   glib-2.0 glib-2.0-native json-glib libdazzle"

GNOMEBASEBUILDCLASS = "meson"
inherit gnomebase gsettings distro_features_check upstream-version-is-even gettext
REQUIRED_DISTRO_FEATURES = "x11 opengl"

SRC_URI = "${GNOME_MIRROR}/${GNOMEBN}/${@gnome_verdir("${PV}")}/${GNOMEBN}-${PV}.tar.${GNOME_COMPRESS_TYPE};name=archive \
           file://0002-help-meson.build-disable-the-use-of-yelp.patch \
           "
SRC_URI[archive.md5sum] = "fd05702b1c9bcb6a0633de54c4a6ccd2"
SRC_URI[archive.sha256sum] = "76cdb8db6af2da8c3371a73e2dfd5ba64867b5f1e65fa9621f35348a42ff0d17"

EXTRA_OEMESON += " -Ddistributor_name=${DISTRO}"

FILES_${PN} += "${datadir}/dbus-1 ${datadir}/gnome-shell/search-providers ${datadir}/metainfo"
RDEPENDS_${PN} = "iso-codes adwaita-icon-theme gsettings-desktop-schemas"
