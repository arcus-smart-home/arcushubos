# We want to build a static version without the need for tcl as it
#  adds over 1M to the image size

EXTRA_OEMAKE = "static"

RDEPENDS_${PN} = ""

do_install() {
    oe_runmake DESTDIR=${D} install-static
}
