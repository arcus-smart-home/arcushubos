#
# Support for Debian pre-build OpenJDK 8 binary package
#

#
# Copyright 2019 Arcus Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

SUMMARY = "Debian pre-build armhf OpenJDK 8 binaries"
PR = "r1"
S = "${WORKDIR}"

DEPENDS = "zip-native"

OPENJDK_VERSION = "8u252-b09-1~deb9u1"

libdir_jvm ?= "${libdir}/jvm"
JDK_HOME = "${libdir_jvm}/java-8-openjdk"
BIN_DIR = "${S}/openjdk-${OPENJDK_VERSION}"

LICENSE = "GPLv2"
LIC_FILES_CHKSUM = "file://${BIN_DIR}/usr/lib/jvm/java-8-openjdk-armhf/ASSEMBLY_EXCEPTION;md5=d94f7c92ff61c5d3f8e9433f76e39f74"

OPENJDK_DEBIAN_URL = "http://security.debian.org/debian-security/pool/updates/main/o/openjdk-8/"

JDK_FILE = "openjdk-8-jdk-headless_${OPENJDK_VERSION}_armhf.deb"
JDK_URI = "${OPENJDK_DEBIAN_URL}/${JDK_FILE};name=jdk;unpack=false"
SRC_URI[jdk.md5sum] = "d30a8782777739bda3739a73fa3e2849"
SRC_URI[jdk.sha256sum] = "c855da94221354ba498ede54e7d75a4e73d670d2e90561fa5e8971b8d52d17bc"

JRE_FILE = "openjdk-8-jre-headless_${OPENJDK_VERSION}_armhf.deb"
JRE_URI = "${OPENJDK_DEBIAN_URL}/${JRE_FILE};name=jre;unpack=false"
SRC_URI[jre.md5sum] = "ab7c6cfbe373e78d2deaa5fc87f9ba87"
SRC_URI[jre.sha256sum] = "5fce1c0033bb52634bd40965677b4c51c12a946523cee8cd35e28469c7dea234"

# Files we override
SRC_URI = " \
	${JDK_URI} \
	${JRE_URI} \
	file://iris-java.security \
	"

FILES_${PN} += "/usr/lib/jvm/java-8-openjdk"

# Ignore warnings
ALLOW_EMPTY_${PN} = "1"
INSANE_SKIP_${PN} = "installed-vs-shipped "

# Avoid "was already stripped, this will prevent future debugging!" errors
INSANE_SKIP_${PN} = "already-stripped"

do_unpack_extract_submodules () {
    mkdir -p "${BIN_DIR}"
    cd "${BIN_DIR}"
    /usr/bin/dpkg-deb -x ${WORKDIR}/${JDK_FILE} .
    /usr/bin/dpkg-deb -x ${WORKDIR}/${JRE_FILE} .
}

do_unpack[postfuncs] += "do_unpack_extract_submodules"


do_install() {
	bbnote "Installing from ${BIN_DIR} to ${D}${JDK_HOME}"
	install -d ${D}${libdir_jvm}
	cp -R ${BIN_DIR}/usr/lib/jvm/java-8-openjdk-armhf/ ${D}${JDK_HOME}
	chmod u+rw -R ${D}${JDK_HOME}

	# Remove top-level items we don't need on the hub
	rm -rf ${D}${JDK_HOME}/docs
	rm -rf ${D}${JDK_HOME}/include
	rm -rf ${D}${JDK_HOME}/man
	rm -rf ${D}${JDK_HOME}/sample
	rm -rf ${D}${JDK_HOME}/src.zip

	# Remove unneeded top-level binaries
        rm -rf ${D}${JDK_HOME}/bin/extcheck
        rm -rf ${D}${JDK_HOME}/bin/idlj
        rm -rf ${D}${JDK_HOME}/bin/jar
        rm -rf ${D}${JDK_HOME}/bin/jarsigner
        rm -rf ${D}${JDK_HOME}/bin/java
        rm -rf ${D}${JDK_HOME}/bin/javac
        rm -rf ${D}${JDK_HOME}/bin/javadoc
        rm -rf ${D}${JDK_HOME}/bin/javah
        rm -rf ${D}${JDK_HOME}/bin/javap
        rm -rf ${D}${JDK_HOME}/bin/java-rmi.cgi
        rm -rf ${D}${JDK_HOME}/bin/jcmd
        rm -rf ${D}${JDK_HOME}/bin/jconsole
        rm -rf ${D}${JDK_HOME}/bin/jdb
        rm -rf ${D}${JDK_HOME}/bin/jdeps
        rm -rf ${D}${JDK_HOME}/bin/jhat
        rm -rf ${D}${JDK_HOME}/bin/jjs
        rm -rf ${D}${JDK_HOME}/bin/jps
        rm -rf ${D}${JDK_HOME}/bin/jrunscript
        rm -rf ${D}${JDK_HOME}/bin/jsadebugd
        rm -rf ${D}${JDK_HOME}/bin/jstat
        rm -rf ${D}${JDK_HOME}/bin/jstatd
        rm -rf ${D}${JDK_HOME}/bin/keytool
        rm -rf ${D}${JDK_HOME}/bin/native2ascii
        rm -rf ${D}${JDK_HOME}/bin/orbd
        rm -rf ${D}${JDK_HOME}/bin/pack200
        rm -rf ${D}${JDK_HOME}/bin/rmic
        rm -rf ${D}${JDK_HOME}/bin/rmid
        rm -rf ${D}${JDK_HOME}/bin/rmiregistry
        rm -rf ${D}${JDK_HOME}/bin/schemagen
        rm -rf ${D}${JDK_HOME}/bin/serialver
        rm -rf ${D}${JDK_HOME}/bin/servertool
        rm -rf ${D}${JDK_HOME}/bin/tnameserv
        rm -rf ${D}${JDK_HOME}/bin/unpack200
        rm -rf ${D}${JDK_HOME}/bin/wsgen
        rm -rf ${D}${JDK_HOME}/bin/wsimport
        rm -rf ${D}${JDK_HOME}/bin/xjc

	# Remove unneeded top-level libraries
        rm -rf ${D}${JDK_HOME}/lib/ct.sym
        rm -rf ${D}${JDK_HOME}/lib/dt.jar
        rm -rf ${D}${JDK_HOME}/lib/ir.idl
        rm -rf ${D}${JDK_HOME}/lib/jexec
        rm -rf ${D}${JDK_HOME}/lib/orb.idl
        rm -rf ${D}${JDK_HOME}/lib/arm/libjawt.so

        # Remove unneeded jre binaries
        rm -rf ${D}${JDK_HOME}/jre/bin/jjs
        rm -rf ${D}${JDK_HOME}/jre/bin/orbd
        rm -rf ${D}${JDK_HOME}/jre/bin/rmid
        rm -rf ${D}${JDK_HOME}/jre/bin/rmiregistry
        rm -rf ${D}${JDK_HOME}/jre/bin/servertool
        rm -rf ${D}${JDK_HOME}/jre/bin/tnameserv

        # Removing graphics related files saves 3 MB.
        rm -rf ${D}${JDK_HOME}/jre/lib/psfont*
        rm -rf ${D}${JDK_HOME}/jre/lib/cmm
        rm -rf ${D}${JDK_HOME}/jre/lib/images
        rm -rf ${D}${JDK_HOME}/jre/lib/accessibility.properties
        rm -rf ${D}${JDK_HOME}/jre/lib/calendars.properties
        rm -rf ${D}${JDK_HOME}/jre/lib/flavormap.properties
        rm -rf ${D}${JDK_HOME}/jre/lib/hijrah-config-umalqura.properties
        rm -rf ${D}${JDK_HOME}/jre/lib/sound.properties
        rm -rf ${D}${JDK_HOME}/jre/lib/swing.properties
        rm -rf ${D}${JDK_HOME}/jre/lib/ext/cldrdata.jar
        rm -rf ${D}${JDK_HOME}/jre/lib/ext/localedata.jar
        rm -rf ${D}${JDK_HOME}/jre/lib/ext/nashorn.jar
        rm -rf ${D}${JDK_HOME}/jre/lib/ext/jaccess.jar
        rm -rf ${D}${JDK_HOME}/jre/lib/arm/libawt_headless.so
        rm -rf ${D}${JDK_HOME}/jre/lib/arm/libawt.so
        rm -rf ${D}${JDK_HOME}/jre/lib/arm/libawt_xawt.so
        rm -rf ${D}${JDK_HOME}/jre/lib/arm/libfontmanager.so
        rm -rf ${D}${JDK_HOME}/jre/lib/arm/libhprof.so
        rm -rf ${D}${JDK_HOME}/jre/lib/arm/libjava_crw_demo.so
        rm -rf ${D}${JDK_HOME}/jre/lib/arm/libjavajpeg.so
        rm -rf ${D}${JDK_HOME}/jre/lib/arm/libjavalcms.so
        # Leave the libjawt.so library, needed for TinyB package
        # rm -rf ${D}${JDK_HOME}/jre/lib/arm/libjawt.so
        rm -rf ${D}${JDK_HOME}/jre/lib/arm/libjsound.so
        rm -rf ${D}${JDK_HOME}/jre/lib/arm/libmlib_image.so

	# Remove man pages
        rm -rf ${D}${JDK_HOME}/jre/man

        # Remove cacerts file - will install separately with
        #  ca-certificates package
        rm -rf ${D}${JDK_HOME}/jre/lib/security/cacerts

        # Removing unneeded packages from rt.jar saves ~15 MB.
        mkdir ${D}${JDK_HOME}/jre/lib/rt_repackage
        cd ${D}${JDK_HOME}/jre/lib/rt_repackage
                /usr/bin/jar xf ../rt.jar
                rm -rf  java/applet \
                        java/awt \
                        java/rmi \
                        javax/imageio \
                        javax/accessibility \
                        javax/swing \
                        javax/rmi \
                        javax/print \
                        javax/sound \
                        javax/management/remote \
                        org/omg \
                        com/sun/imageio \
                        com/sun/accessibility \
                        com/sun/swing \
                        com/sun/java_cup \
                        com/sun/awt \
                        com/sun/rmi \
                        com/sun/corba \
                        com/sun/demo \
                        com/sun/java/swing \
                        com/sun/java/browser \
                        com/sun/org/glassfish \
                        com/sun/org/omg \
                        com/sun/jmx/remote \
                        com/sun/jndi \
                        com/sun/media \
                        sun/swing \
                        sun/awt \
                        sun/rmi \
                        sun/corba \
                        sun/applet \
                        sun/java2d \
                        sun/font \
                        sun/print \
                        sun/net/ftp

        # Removing XML support from rt.jar saves a substantial amount of
        # space - these items were never used...
	 rm -rf \
              com/sun/org/apache/xalan \
              com/sun/org/apache/xml \
              com/sun/org/apache/xpath \
              org/jcp

        # We were using these xml items, but agent will remove these ...
        # rm -rf com/sun/xml \
        #      com/sun/org/apache/xerces \
        #      javax/xml \
        #      org/xml \
        #      org/w3c

              find . -iname '*swing*' -exec rm -f {} \;
              find . -iname '*awt*' -exec rm -f {} \;

              rm ../rt.jar
              zip -D -X -9 -q -r ../rt.jar .
        cd -
        rm -rf ${D}${JDK_HOME}/jre/lib/rt_repackage

        # Removing unneeded packages from resources.jar saves 564 KB.
        mkdir ${D}${JDK_HOME}/jre/lib/resources_repackage
        cd ${D}${JDK_HOME}/jre/lib/resources_repackage
                /usr/bin/jar xf ../resources.jar
                rm -rf  sun/print \
                        sun/rmi \
                        com/sun/imageio \
                        com/sun/corba \
                        com/sun/jndi \
                        com/sun/java/swing \
                        javax/swing \
                        META-INF/services/sun.java2d* \
                        META-INF/services/javax.print* \
                        META-INF/services/*xjc*

                rm ../resources.jar
                zip -D -X -9 -q -r ../resources.jar .
        cd -
        rm -rf ${D}${JDK_HOME}/jre/lib/resources_repackage

        # Removing unneeded packages from tools.jar saves over 6M
        mkdir ${D}${JDK_HOME}/lib/resources_repackage
        cd ${D}${JDK_HOME}/lib/resources_repackage
                /usr/bin/jar xf ../tools.jar
                rm -rf  com/sun/codemodel \
                        com/sun/doclint \
                        com/sun/jarsigner \
                        com/sun/istack \
                        com/sun/javadoc \
                        com/sun/jdeps \
                        com/sun/jdi \
                        com/sun/mirror \
                        com/sun/source \
                        com/sun/tools/apt \
                        com/sun/tools/classfile \
                        com/sun/tools/corba \
                        com/sun/tools/doclets \
                        com/sun/tools/example \
                        com/sun/tools/extcheck \
                        com/sun/tools/hat \
                        com/sun/tools/internal \
                        com/sun/tools/jar \
                        com/sun/tools/javac \
                        com/sun/tools/javadoc \
                        com/sun/tools/javah \
                        com/sun/tools/javap \
                        com/sun/tools/jdi \
                        com/sun/tools/script \
                        com/sun/xml \
                        org \
                        sun/applet \
                        sun/jvmstat \
                        sun/rmi \
                        sun/tools/asm \
                        sun/tools/java \
                        sun/tools/javac \
                        sun/tools/jcmd \
                        sun/tools/jstat \
                        sun/tools/jstatd \
                        sun/tools/jps \
                        sun/tools/jar \
                        sun/tools/native2ascii \
                        sun/tools/serialver \
                        sun/tools/tree

                rm ../tools.jar
                zip -D -X -9 -q -r ../tools.jar .
        cd -
        rm -rf ${D}${JDK_HOME}/lib/resources_repackage

        # Removing unneeded packages from charsets.jar saves 2 MB.
        mkdir ${D}${JDK_HOME}/jre/lib/charsets_repackage
        cd ${D}${JDK_HOME}/jre/lib/charsets_repackage
                /usr/bin/jar xf ../charsets.jar
            rm -rf sun/awt
        find . -type f |grep -iv iso |grep -iv ascii |grep -iv double |xargs rm -rf

                rm ../charsets.jar
                zip -D -X -9 -q -r ../charsets.jar .
        cd -
        rm -rf ${D}${JDK_HOME}/jre/lib/charsets_repackage

	# Symbolically linking lib/arm/jli/libjli.so to jre saves 20 KB.
	rm ${D}${JDK_HOME}/lib/arm/jli/libjli.so
	ln -s ${JDK_HOME}/jre/lib/arm/jli/libjli.so ${D}${JDK_HOME}/lib/arm/jli/libjli.so
	ln -s ${JDK_HOME}/jre/lib/arm/jli/libjli.so ${D}${JDK_HOME}/jre/lib/arm/libjli.so

	# JRE is a subset of JDK. So to save space and resemble what the BIG distros
	# do we create symlinks from the JDK binaries to their counterparts in the
	# JRE folder (which have to exist by that time b/c of dependencies).
	for F in `find ${D}${JDK_HOME}/jre/bin -type f`
	do
		bf=`basename $F`
		bbnote "replace:" $bf
		rm -f ${D}${JDK_HOME}/bin/$bf
		ln -s ${JDK_HOME}/jre/bin/$bf ${D}${JDK_HOME}/bin/$bf
	done

	# Create /usr/bin/java link as well
	install -d ${D}${bindir}
	ln -s ${JDK_HOME}/jre/bin/java ${D}${bindir}/java

        # workaround for shared library searching
	ln -sf ${JDK_HOME}/jre/lib/arm/client/libjvm.so ${D}${JDK_HOME}/jre/lib/arm/

	# Add in etc files
	install -m644 ${BIN_DIR}/etc/java-8-openjdk/jvm-armhf.cfg ${D}${JDK_HOME}/jre/lib/arm/jvm.cfg
	install -m644 ${BIN_DIR}/etc/java-8-openjdk/content-types.properties ${D}${JDK_HOME}/jre/lib/
	install -m644 ${BIN_DIR}/etc/java-8-openjdk/logging.properties ${D}${JDK_HOME}/jre/lib/
	install -m644 ${BIN_DIR}/etc/java-8-openjdk/net.properties ${D}${JDK_HOME}/jre/lib/
	install -m644 ${BIN_DIR}/etc/java-8-openjdk/swing.properties ${D}${JDK_HOME}/jre/lib/
	install -m644 ${BIN_DIR}/etc/java-8-openjdk/management/jmxremote.access ${D}${JDK_HOME}/jre/lib/management/
	install -m644 ${BIN_DIR}/etc/java-8-openjdk/management/jmxremote.password ${D}${JDK_HOME}/jre/lib/management/
	install -m644 ${BIN_DIR}/etc/java-8-openjdk/management/management.properties ${D}${JDK_HOME}/jre/lib/management/
	install -m644 ${BIN_DIR}/etc/java-8-openjdk/management/snmp.acl ${D}${JDK_HOME}/jre/lib/management/
	install -m644 ${BIN_DIR}/etc/java-8-openjdk/security/blacklisted.certs ${D}${JDK_HOME}/jre/lib/security/
	install -m644 ${BIN_DIR}/etc/java-8-openjdk/security/java.policy ${D}${JDK_HOME}/jre/lib/security/

	# Use our own Java security settings in we want to override something
        install -m644 ${WORKDIR}/iris-java.security  ${D}${JDK_HOME}/jre/lib/security/java.security
	rm -rf ${D}${JDK_HOME}/jre/lib/security/nss.cfg
}

# FIXME: we shouldn't override the QA!
do_package_qa() {
	pwd
}
