SUMMARY = "Simplifies building parse types based on the parse module"
LICENSE = "BSD"
LIC_FILES_CHKSUM = "file://LICENSE;md5=d07323820cca0f1d192cbbf8a0516f95"
PYPI_PACKAGE = "parse_type"

SRC_URI[md5sum] = "b5fa59e45965d1b2896023742df2e707"
SRC_URI[sha256sum] = "f596bdc75d3dd93036fbfe3d04127da9f6df0c26c36e01e76da85adef4336b3c"

RDEPENDS_${PN} += "python-parse"

inherit pypi setuptools
