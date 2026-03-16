#
# Support for Adoptium Temurin pre-built OpenJDK 8 binary package
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

SUMMARY = "Adoptium Temurin pre-built armhf OpenJDK 8 binaries"
PR = "r1"
S = "${WORKDIR}"

DEPENDS = "zip-native unzip-native"

libdir_jvm ?= "${libdir}/jvm"
JDK_HOME = "${libdir_jvm}/java-8-openjdk"
BIN_DIR = "${WORKDIR}/jdk"

LICENSE = "GPLv2"
LIC_FILES_CHKSUM = "file://${BIN_DIR}/ASSEMBLY_EXCEPTION;md5=d94f7c92ff61c5d3f8e9433f76e39f74"

SRC_URI = " \
	https://github.com/adoptium/temurin8-binaries/releases/download/jdk8u482-b08/OpenJDK8U-jdk_arm_linux_hotspot_8u482b08.tar.gz \
	file://iris-java.security \
	"
SRC_URI[sha256sum] = "1d0d16394e2fe637f9eb8e73e63ea6fe9ceee98337c0527aa058cee777ad638a"

FILES:${PN} += "/usr/lib/jvm/java-8-openjdk"

ALLOW_EMPTY:${PN} = "1"
INSANE_SKIP:${PN} = "installed-vs-shipped already-stripped"

# Pre-built binaries — skip rpmdeps ELF scanning so internal cross-references
# (e.g. libjawt→libawt, libsplashscreen→libX11) don't become RPM Requires.
SKIP_FILEDEPS:${PN} = "1"

# Rename the date-stamped extracted directory to a deterministic name
do_unpack_fixup () {
    cd "${WORKDIR}"
    mv jdk8u482-b08-aarch32-* jdk || true
}

do_unpack[postfuncs] += "do_unpack_fixup"


do_install() {
	bbnote "Installing from ${BIN_DIR} to ${D}${JDK_HOME}"
	install -d ${D}${libdir_jvm}
	cp -R ${BIN_DIR}/ ${D}${JDK_HOME}
	chmod u+rw -R ${D}${JDK_HOME}

	# Remove top-level items we don't need on the hub
	rm -rf ${D}${JDK_HOME}/docs
	rm -rf ${D}${JDK_HOME}/include
	rm -rf ${D}${JDK_HOME}/man
	rm -rf ${D}${JDK_HOME}/sample
	rm -rf ${D}${JDK_HOME}/src.zip
	rm -rf ${D}${JDK_HOME}/ASSEMBLY_EXCEPTION
	rm -rf ${D}${JDK_HOME}/DISCLAIMER
	rm -rf ${D}${JDK_HOME}/LICENSE
	rm -rf ${D}${JDK_HOME}/THIRD_PARTY_README
	rm -rf ${D}${JDK_HOME}/NOTICE
	rm -rf ${D}${JDK_HOME}/release
	rm -rf ${D}${JDK_HOME}/jre/LICENSE
	rm -rf ${D}${JDK_HOME}/jre/ASSEMBLY_EXCEPTION
	rm -rf ${D}${JDK_HOME}/jre/THIRD_PARTY_README

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
        rm -rf ${D}${JDK_HOME}/bin/clhsdb
        rm -rf ${D}${JDK_HOME}/bin/hsdb
        rm -rf ${D}${JDK_HOME}/bin/jfr
        rm -rf ${D}${JDK_HOME}/bin/jinfo
        rm -rf ${D}${JDK_HOME}/bin/jmap
        rm -rf ${D}${JDK_HOME}/bin/jstack

	# Remove unneeded top-level libraries
        rm -rf ${D}${JDK_HOME}/lib/ct.sym
        rm -rf ${D}${JDK_HOME}/lib/dt.jar
        rm -rf ${D}${JDK_HOME}/lib/ir.idl
        rm -rf ${D}${JDK_HOME}/lib/jexec
        rm -rf ${D}${JDK_HOME}/lib/orb.idl
        rm -rf ${D}${JDK_HOME}/lib/aarch32/libjawt.so
        rm -rf ${D}${JDK_HOME}/lib/jconsole.jar

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
        rm -rf ${D}${JDK_HOME}/jre/lib/aarch32/libawt_headless.so
        rm -rf ${D}${JDK_HOME}/jre/lib/aarch32/libawt.so
        rm -rf ${D}${JDK_HOME}/jre/lib/aarch32/libawt_xawt.so
        rm -rf ${D}${JDK_HOME}/jre/lib/aarch32/libfontmanager.so
        rm -rf ${D}${JDK_HOME}/jre/lib/aarch32/libhprof.so
        rm -rf ${D}${JDK_HOME}/jre/lib/aarch32/libjava_crw_demo.so
        rm -rf ${D}${JDK_HOME}/jre/lib/aarch32/libjavajpeg.so
        rm -rf ${D}${JDK_HOME}/jre/lib/aarch32/libjavalcms.so
        # Leave the libjawt.so library, needed for TinyB package
        # rm -rf ${D}${JDK_HOME}/jre/lib/aarch32/libjawt.so
        rm -rf ${D}${JDK_HOME}/jre/lib/aarch32/libjsound.so
        rm -rf ${D}${JDK_HOME}/jre/lib/aarch32/libmlib_image.so
        rm -rf ${D}${JDK_HOME}/jre/lib/aarch32/libsplashscreen.so
        rm -rf ${D}${JDK_HOME}/jre/lib/aarch32/liblcms.so

        # Remove GUI binaries not needed on headless hub
        rm -rf ${D}${JDK_HOME}/bin/appletviewer
        rm -rf ${D}${JDK_HOME}/bin/policytool
        rm -rf ${D}${JDK_HOME}/jre/bin/policytool

	# Remove man pages
        rm -rf ${D}${JDK_HOME}/jre/man

        # Remove cacerts file - will install separately with
        #  ca-certificates package
        rm -rf ${D}${JDK_HOME}/jre/lib/security/cacerts

        # Removing unneeded packages from rt.jar saves ~15 MB.
        mkdir ${D}${JDK_HOME}/jre/lib/rt_repackage
        cd ${D}${JDK_HOME}/jre/lib/rt_repackage
                unzip -q -o ../rt.jar
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
                unzip -q -o ../resources.jar
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
                unzip -q -o ../tools.jar
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
                unzip -q -o ../charsets.jar
            rm -rf sun/awt
        find . -type f |grep -iv iso |grep -iv ascii |grep -iv double |xargs rm -rf

                rm ../charsets.jar
                zip -D -X -9 -q -r ../charsets.jar .
        cd -
        rm -rf ${D}${JDK_HOME}/jre/lib/charsets_repackage

	# Symbolically linking lib/aarch32/jli/libjli.so to jre saves 20 KB.
	rm -f ${D}${JDK_HOME}/lib/aarch32/jli/libjli.so
	ln -s ${JDK_HOME}/jre/lib/aarch32/jli/libjli.so ${D}${JDK_HOME}/lib/aarch32/jli/libjli.so
	ln -s ${JDK_HOME}/jre/lib/aarch32/jli/libjli.so ${D}${JDK_HOME}/jre/lib/aarch32/libjli.so

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
	ln -sf ${JDK_HOME}/jre/lib/aarch32/client/libjvm.so ${D}${JDK_HOME}/jre/lib/aarch32/

	# Rename management templates to their expected names
	mv -f ${D}${JDK_HOME}/jre/lib/management/jmxremote.password.template \
	      ${D}${JDK_HOME}/jre/lib/management/jmxremote.password || true
	mv -f ${D}${JDK_HOME}/jre/lib/management/snmp.acl.template \
	      ${D}${JDK_HOME}/jre/lib/management/snmp.acl || true

	# Use our own Java security settings in we want to override something
        install -m644 ${WORKDIR}/iris-java.security  ${D}${JDK_HOME}/jre/lib/security/java.security
	rm -rf ${D}${JDK_HOME}/jre/lib/security/nss.cfg
}

# FIXME: we shouldn't override the QA!
do_package_qa() {
	pwd
}
