# IRIS HubOS Build Instructions for OSS Redistribution

The development environment for the IRIS HubOS is currently based on the
Yocto 2.4.3 "Rocko" release.  Our proprietary hardware is similar to that
of the BeagleBone Black for our second generation Hub, and a custom
NXP imx6DualLite design for the 3rd generation Hub.

Our changes to the OSS packages used are marked with "IRIS Change" additions
to the files touched.  A list of changes can be found in the [ChangeLog_oss.txt](ChangeLog_oss.txt)
file.



## Instructions for build environment set up

Please use a Yocto Supported Operating system. For Yocto 2.4.3, this is **CentOS 7**, **Debian 9**, Fedora 26, Fedora 27, OpenSuse 42.3, **Ubuntu 16.04**, and Ubuntu 17.10.

As with any Yocto project, you should be using a fairly capable system (e.g. modern core "i" series processor, 25GB or more of disk space, and ideally an SSD) to achieve reasoanble build times.

Follow the Yocto environment set up instructions in "The Packages" section at:

http://www.yoctoproject.org/docs/current/yocto-project-qs/yocto-project-qs.html

For a Ubuntu distribution, the following tools are needed:

`$ sudo apt-get install gawk wget git-core diffstat unzip texinfo gcc-multilib \
build-essential chrpath socat libsdl1.2-dev xterm python chrpath`

We have only tested building under Ubuntu, but other environments should
also work.

We have added JDK support to the base Yocto build, so you will also
need to install the following (assuming Ubuntu environment):

`$ sudo apt-get install openjdk-8-jdk galternatives srecord`

Run 'galternatives' and make sure 'keytool' and 'jar" are configured to the
java8-openjdk package which was just installed.

The build outputs are put in /tftpboot to simplify finding the firmware
images.  Please add this directory - actual tftp server support is not needed,
though.

```
$ sudo mkdir /tftpboot
$ sudo chmod -R 777 /tftpboot
$ sudo chown -R nobody /tftpboot
```

Before running the build, either create a top-level /build directory, or modify
the configuration in build/conf/local.conf to comment out this line or change
to another location:

`SSTATE_DIR ?= "/build/sstate-cache"`



## Makefile Targets

The following targets are valid:

* hubv3           - builds release image for v3 hub
* hubv3-dev       - builds development image with added tools, etc. for v3 hub
* hubv3-clean     - cleans up v3 build outputs. Would need to remove
                  /build/sstate-cache to truely force a complete rebuild
* hubv3-distclean - Removes all v3 files from "clean" as well as downloaded files
                  to mimic starting from scratch

* hubv2           - builds release image for v2 hub
* hubv2-dev       - builds development image with added tools, etc. for v2 hub
* hubv2-clean     - cleans up v2 build outputs. Would need to remove
                  /build/sstate-cache to truely force a complete rebuild
* hubv2-distclean - Removes all v2 files from "clean" as well as downloaded files
                  to mimic starting from scratch


These builds will take a hour or so (depending on your machine) to complete
as it has to build all the cross-compiler toolchains as well as all packages
in the build.

When complete, the outputs will be in build-ti/tmp/deploy/images for v2
hub builds, and build-fsl/tmp/deploy/images for v3 hub builds.



## Build Output Files

* i2hubos_update.bin - v2 image without the build header (which includes
                     checksum, signature, etc)
* i2hubos_update_bootloader.bin - Above, but with v2 boot loader files as well
* hubOS_X.Y.Z.xxx.bin - v2 Release image with Java support with release boot
                      loader
* hubBL_X.Y.Z.xxx.bin - v2 Release image with Java support, plus development
                      boot loader files

* i2hubosv3_update.bin - v3 image without the build header (which includes
                       checksum, signature, etc)
* i2hubosv3_update_bootloader.bin - Above, but with v3 boot loader files as well
* hubOSv3_X.Y.Z.xxx.bin - v3 Release image with Java support with release boot
                        loader
* hubBLv3_X.Y.Z.xxx.bin - v3 Release image with Java support, plus development
                        boot loader files

# Release Testing

```
$ scp /tftpboot/hubOS_X.Y.Z.xxx.bin root@your_v2_hub:/tmp
$ ssh root@your_hub_ip
$ install -f file:///tmp/hubOS_X.Y.Z.xxx.bin
```
# Common Tasks

## Updating Firmware Version

Bump the version number in [meta-iris/recipes-core/iris-lib/files/irisversion.h](meta-iris/recipes-core/iris-lib/files/irisversion.h)

## Updating the Agent

Bump the version number in [meta-iris/recipes-core/iris-agent/iris-agent.bb#L40](meta-iris/recipes-core/iris-agent/iris-agent.bb#L40) and change the hashes of the target tar.gz file:

# Development

For development, it is not necessary to use the complete images as the
validation takes additional time.  Also, since the boot loader files will
seldom change, using the i2hubos_update.bin (or i2hubosv3_update.bin for v3
hub) is all that is necessary in most cases.   To install this file, do the
following:


```
$ scp /tftpboot/i2hubos_update.bin root@your_hub_ip:/tmp
$ ssh root@your_hub_ip
$ fwinstall /tmp/i2hubos_update.bin
```

Once this install completes reboot the hub so the new firmware is used.

# Password

The default root password for hubOS 3.x, regardless of hardare revision or platform is `zm{[*f6gB5X($]R9`.
