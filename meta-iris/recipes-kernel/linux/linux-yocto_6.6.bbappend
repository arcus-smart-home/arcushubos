FILESEXTRAPATHS:prepend := "${THISDIR}/linux-6.x:"

# Clear out the kernel extra features to make sure netfilter support doesn't
#  get added back in!
KERNEL_EXTRA_FEATURES = ""
KERNEL_FEATURES:append = ""

# Kernel branch/machine config (replaces masked meta-yocto-bsp bbappend)
KBRANCH:beaglebone-yocto = "v6.6/standard/beaglebone"
KMACHINE:beaglebone-yocto ?= "beaglebone"
SRCREV_machine:beaglebone-yocto = "9bd5232ea463156cf378d6b99b3460b8826b7dba"
LINUX_VERSION:beaglebone-yocto = "6.6.127"
LINUX_VERSION = "6.6.127"
PV = "${LINUX_VERSION}+git${SRCPV}"
COMPATIBLE_MACHINE:beaglebone-yocto = "beaglebone-yocto"

# Create a uImage output file — old U-Boot on Hub V2 requires this
KERNEL_IMAGETYPE = "uImage"
KERNEL_EXTRA_ARGS += "LOADADDR=0x80008000"

SRC_URI += " \
	file://defconfig \
	file://adc.cfg \
	file://fs.cfg \
	file://leds.cfg \
	file://pwm.cfg \
	file://usb.cfg \
	file://serial.cfg \
	file://debug.cfg \
	file://gpio.cfg \
	file://nosound.cfg \
	file://0001-Iris-dtsi-config-changes.patch \
	file://0002-Disable-Ethernet-MDIX.patch \
	"
