#
# Support for Adoptium Temurin pre-built OpenJDK 11 binary package
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

SUMMARY = "Adoptium Temurin pre-built armhf OpenJDK 11 binaries"
PR = "r1"
S = "${WORKDIR}"

DEPENDS = "zip-native unzip-native"


libdir_jvm ?= "${libdir}/jvm"
JDK_HOME = "${libdir_jvm}/java-11-openjdk"
BIN_DIR = "${WORKDIR}/jdk"

LICENSE = "GPLv2"
LIC_FILES_CHKSUM = "file://${BIN_DIR}/legal/java.base/ASSEMBLY_EXCEPTION;md5=d94f7c92ff61c5d3f8e9433f76e39f74"

SRC_URI = " \
	https://github.com/adoptium/temurin11-binaries/releases/download/jdk-11.0.30%2B7/OpenJDK11U-jdk_arm_linux_hotspot_11.0.30_7.tar.gz \
	file://iris-java.security \
	"
SRC_URI[sha256sum] = "1ef020c2215f3169c7610df573581806c58f00a0a1d512fd945a2687cbed1173"

FILES:${PN} += "/usr/lib/jvm/java-11-openjdk"

ALLOW_EMPTY:${PN} = "1"
INSANE_SKIP:${PN} = "installed-vs-shipped already-stripped"

# Pre-built binaries — skip rpmdeps ELF scanning so internal cross-references
# don't become RPM Requires.
SKIP_FILEDEPS:${PN} = "1"

# Rename the date-stamped extracted directory to a deterministic name
do_unpack_fixup () {
    cd "${WORKDIR}"
    mv jdk-11.0.30+7 jdk || true
}

do_unpack[postfuncs] += "do_unpack_fixup"

# Modules required by the Arcus hub agent (determined via jdeps scan).
# java.desktop is needed for java.beans (used by Guava/Jackson) despite
# running headless.  Crypto/naming modules are needed for TLS and DNS.
OPENJDK_MODULES = "java.base,java.datatransfer,java.desktop,java.instrument,java.logging,java.management,java.naming,java.prefs,java.rmi,java.security.jgss,java.security.sasl,java.sql,java.xml,java.xml.crypto,jdk.crypto.cryptoki,jdk.crypto.ec,jdk.management,jdk.management.agent,jdk.naming.dns,jdk.net,jdk.unsupported,jdk.zipfs"

do_install() {
	bbnote "Building custom JDK 11 runtime with jlink..."

	# Use host jlink with the ARM jmods to create a trimmed runtime.
	# --compress=2 applies ZIP compression to the modules image.
	# --vm=client uses the client VM for faster startup (JDK 8 did the same).
	jlink \
		--module-path ${BIN_DIR}/jmods \
		--add-modules ${OPENJDK_MODULES} \
		--compress=2 \
		--vm=client \
		--no-header-files --no-man-pages \
		--output ${WORKDIR}/jdk-custom

	bbnote "Installing custom runtime to ${D}${JDK_HOME}"
	install -d ${D}${libdir_jvm}
	cp -R ${WORKDIR}/jdk-custom/ ${D}${JDK_HOME}
	chmod u+rw -R ${D}${JDK_HOME}

	# Remove unneeded binaries (keep only java and keytool)
	for f in ${D}${JDK_HOME}/bin/*; do
		bf=$(basename "$f")
		case "$bf" in
			java|keytool) ;;
			*) rm -f "$f" ;;
		esac
	done

	# Remove graphics/desktop libraries not needed on headless hub
	rm -f ${D}${JDK_HOME}/lib/libawt_headless.so
	rm -f ${D}${JDK_HOME}/lib/libawt.so
	rm -f ${D}${JDK_HOME}/lib/libawt_xawt.so
	rm -f ${D}${JDK_HOME}/lib/libfontmanager.so
	rm -f ${D}${JDK_HOME}/lib/libfreetype.so
	rm -f ${D}${JDK_HOME}/lib/libjavajpeg.so
	rm -f ${D}${JDK_HOME}/lib/libjsound.so
	rm -f ${D}${JDK_HOME}/lib/liblcms.so
	rm -f ${D}${JDK_HOME}/lib/libmlib_image.so
	rm -f ${D}${JDK_HOME}/lib/libsplashscreen.so

	# Remove font/graphics config files
	rm -f ${D}${JDK_HOME}/lib/psfont*
	rm -f ${D}${JDK_HOME}/conf/sound.properties

	# Remove cacerts file — will install separately with ca-certificates package
	rm -f ${D}${JDK_HOME}/lib/security/cacerts

	# Remove debug agent (not needed in production)
	rm -f ${D}${JDK_HOME}/lib/libjdwp.so

	# Remove public suffix list (cookie domain validation — not needed on hub)
	rm -f ${D}${JDK_HOME}/lib/security/public_suffix_list.dat

	# workaround for shared library searching
	ln -sf ${JDK_HOME}/lib/client/libjvm.so ${D}${JDK_HOME}/lib/

	# Rename management templates to their expected names
	mv -f ${D}${JDK_HOME}/conf/management/jmxremote.password.template \
	      ${D}${JDK_HOME}/conf/management/jmxremote.password || true

	# Use our own Java security settings
	install -m644 ${WORKDIR}/iris-java.security ${D}${JDK_HOME}/conf/security/java.security

	# Create /usr/bin/java link
	install -d ${D}${bindir}
	ln -s ${JDK_HOME}/bin/java ${D}${bindir}/java
}

# FIXME: we shouldn't override the QA!
do_package_qa() {
	pwd
}
