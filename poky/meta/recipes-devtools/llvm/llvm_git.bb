# Copyright (C) 2017 Khem Raj <raj.khem@gmail.com>
# Released under the MIT license (see COPYING.MIT for the terms)

DESCRIPTION = "The LLVM Compiler Infrastructure"
HOMEPAGE = "http://llvm.org"
LICENSE = "NCSA"
SECTION = "devel"

LIC_FILES_CHKSUM = "file://LICENSE.TXT;md5=c6b766a4e85dd28301eeed54a6684648"

DEPENDS = "libffi libxml2 zlib ninja-native llvm-native"

RDEPENDS_${PN}_append_class-target = " ncurses-terminfo"

inherit cmake pkgconfig

PROVIDES += "llvm${PV}"

LLVM_RELEASE = "${PV}"
LLVM_DIR = "llvm${LLVM_RELEASE}"

SRCREV = "d2298e74235598f15594fe2c99bbac870a507c59"

BRANCH = "release/${MAJOR_VERSION}.x"
MAJOR_VERSION = "8"
MINOR_VERSION = "0"
PATCH_VERSION = "0"
SOLIBVER = "1"
PV = "${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_VERSION}"
SRC_URI = "git://github.com/llvm/llvm-project.git;branch=${BRANCH} \
           file://0001-llvm-TargetLibraryInfo-Undefine-libc-functions-if-th.patch \
           file://0002-llvm-allow-env-override-of-exe-path.patch \
          "

S = "${WORKDIR}/git/llvm"

LLVM_INSTALL_DIR = "${WORKDIR}/llvm-install"

def get_llvm_arch(bb, d, arch_var):
    import re
    a = d.getVar(arch_var)
    if   re.match(r'(i.86|athlon|x86.64)$', a):         return 'X86'
    elif re.match(r'arm$', a):                          return 'ARM'
    elif re.match(r'armeb$', a):                        return 'ARM'
    elif re.match(r'aarch64$', a):                      return 'AArch64'
    elif re.match(r'aarch64_be$', a):                   return 'AArch64'
    elif re.match(r'mips(isa|)(32|64|)(r6|)(el|)$', a): return 'Mips'
    elif re.match(r'p(pc|owerpc)(|64)', a):             return 'PowerPC'
    else:
        raise bb.parse.SkipRecipe("Cannot map '%s' to a supported LLVM architecture" % a)

def get_llvm_host_arch(bb, d):
    return get_llvm_arch(bb, d, 'HOST_ARCH')

#
# Default to build all OE-Core supported target arches (user overridable).
#
LLVM_TARGETS ?= "AMDGPU;${@get_llvm_host_arch(bb, d)}"

ARM_INSTRUCTION_SET_armv5 = "arm"
ARM_INSTRUCTION_SET_armv4t = "arm"

EXTRA_OECMAKE += "-DLLVM_ENABLE_ASSERTIONS=OFF \
                  -DLLVM_ENABLE_EXPENSIVE_CHECKS=OFF \
                  -DLLVM_ENABLE_PIC=ON \
                  -DLLVM_BINDINGS_LIST='' \
                  -DLLVM_LINK_LLVM_DYLIB=ON \
                  -DLLVM_ENABLE_FFI=ON \
                  -DLLVM_ENABLE_RTTI=ON \
                  -DFFI_INCLUDE_DIR=$(pkg-config --variable=includedir libffi) \
                  -DLLVM_OPTIMIZED_TABLEGEN=ON \
                  -DLLVM_TARGETS_TO_BUILD='${LLVM_TARGETS}' \
                  -DLLVM_TEMPORARILY_ALLOW_OLD_TOOLCHAIN=ON \
                  -DPYTHON_EXECUTABLE=${HOSTTOOLS_DIR}/python2 \
                  -G Ninja"

EXTRA_OECMAKE_append_class-target = "\
                  -DCMAKE_CROSSCOMPILING:BOOL=ON \
                  -DLLVM_TABLEGEN=${STAGING_BINDIR_NATIVE}/llvm-tblgen${PV} \
                  -DLLVM_CONFIG_PATH=${STAGING_BINDIR_NATIVE}/llvm-config${PV} \
                 "

EXTRA_OECMAKE_append_class-nativesdk = "\
                  -DCMAKE_CROSSCOMPILING:BOOL=ON \
                  -DLLVM_TABLEGEN=${STAGING_BINDIR_NATIVE}/llvm-tblgen${PV} \
                  -DLLVM_CONFIG_PATH=${STAGING_BINDIR_NATIVE}/llvm-config${PV} \
                 "

CXXFLAGS_append_class-target_powerpc = " -mlongcall"

do_configure_prepend() {
# Fix paths in llvm-config
	sed -i "s|sys::path::parent_path(CurrentPath))\.str()|sys::path::parent_path(sys::path::parent_path(CurrentPath))).str()|g" ${S}/tools/llvm-config/llvm-config.cpp
	sed -ri "s#/(bin|include|lib)(/?\")#/\1/${LLVM_DIR}\2#g" ${S}/tools/llvm-config/llvm-config.cpp
	sed -ri "s#lib/${LLVM_DIR}#${baselib}/${LLVM_DIR}#g" ${S}/tools/llvm-config/llvm-config.cpp
}

do_compile() {
	ninja -v ${PARALLEL_MAKE}
}

do_compile_class-native() {
	ninja -v ${PARALLEL_MAKE} llvm-config llvm-tblgen
}

do_install() {
	DESTDIR=${LLVM_INSTALL_DIR} ninja -v install
	install -D -m 0755 ${B}/bin/llvm-config ${D}${libdir}/${LLVM_DIR}/llvm-config

	install -d ${D}${bindir}/${LLVM_DIR}
	cp -r ${LLVM_INSTALL_DIR}${bindir}/* ${D}${bindir}/${LLVM_DIR}/

	install -d ${D}${includedir}/${LLVM_DIR}
	cp -r ${LLVM_INSTALL_DIR}${includedir}/* ${D}${includedir}/${LLVM_DIR}/

	install -d ${D}${libdir}/${LLVM_DIR}

	# The LLVM sources have "/lib" embedded and so we cannot completely rely on the ${libdir} variable
	if [ -d ${LLVM_INSTALL_DIR}${libdir}/ ]; then
		cp -r ${LLVM_INSTALL_DIR}${libdir}/* ${D}${libdir}/${LLVM_DIR}/
	elif [ -d ${LLVM_INSTALL_DIR}${prefix}/lib ]; then
		cp -r ${LLVM_INSTALL_DIR}${prefix}/lib/* ${D}${libdir}/${LLVM_DIR}/
	elif [ -d ${LLVM_INSTALL_DIR}${prefix}/lib64 ]; then
		cp -r ${LLVM_INSTALL_DIR}${prefix}/lib64/* ${D}${libdir}/${LLVM_DIR}/
	fi

	# Remove unnecessary cmake files
	rm -rf ${D}${libdir}/${LLVM_DIR}/cmake

	ln -s ${LLVM_DIR}/libLLVM-${MAJOR_VERSION}${SOLIBSDEV} ${D}${libdir}/libLLVM-${MAJOR_VERSION}${SOLIBSDEV}

	# We'll have to delete the libLLVM.so due to multiple reasons...
	rm -rf ${D}${libdir}/${LLVM_DIR}/libLLVM.so
	rm -rf ${D}${libdir}/${LLVM_DIR}/libLTO.so
}

do_install_class-native() {
	install -D -m 0755 ${B}/bin/llvm-tblgen ${D}${bindir}/llvm-tblgen${PV}
	install -D -m 0755 ${B}/bin/llvm-config ${D}${bindir}/llvm-config${PV}
	install -D -m 0755 ${B}/lib/libLLVM-${MAJOR_VERSION}.so ${D}${libdir}/libLLVM-${MAJOR_VERSION}.so
}

PACKAGES =+ "${PN}-bugpointpasses ${PN}-llvmhello ${PN}-libllvm ${PN}-liboptremarks ${PN}-liblto"

RRECOMMENDS_${PN}-dev += "${PN}-bugpointpasses ${PN}-llvmhello ${PN}-liboptremarks"

FILES_${PN}-bugpointpasses = "\
    ${libdir}/${LLVM_DIR}/BugpointPasses.so \
"

FILES_${PN}-libllvm = "\
    ${libdir}/${LLVM_DIR}/libLLVM-${MAJOR_VERSION}.so \
    ${libdir}/libLLVM-${MAJOR_VERSION}.so \
"

FILES_${PN}-liblto += "\
    ${libdir}/${LLVM_DIR}/libLTO.so.* \
"

FILES_${PN}-liboptremarks += "\
    ${libdir}/${LLVM_DIR}/libOptRemarks.so.* \
"

FILES_${PN}-llvmhello = "\
    ${libdir}/${LLVM_DIR}/LLVMHello.so \
"

FILES_${PN}-dev += " \
    ${libdir}/${LLVM_DIR}/llvm-config \
    ${libdir}/${LLVM_DIR}/libOptRemarks.so \
    ${libdir}/${LLVM_DIR}/libLLVM-${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_VERSION}.so \
"

FILES_${PN}-staticdev += "\
    ${libdir}/${LLVM_DIR}/*.a \
"

INSANE_SKIP_${PN}-libllvm += "dev-so"

BBCLASSEXTEND = "native nativesdk"
