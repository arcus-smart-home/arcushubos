FILESEXTRAPATHS_prepend := "${THISDIR}/linux-4.12:"

# Clear out the kernel extra features to make sure netfilter support doesn't
#  get added back in!
KERNEL_EXTRA_FEATURES = ""
KERNEL_FEATURES_append = ""

# Create a uImage output file to match what we have done in past
KERNEL_IMAGETYPE = "uImage"

SRC_URI += " \
	file://defconfig \
	file://adc.cfg \
	file://fs.cfg \
	file://leds.cfg \
	file://pwm.cfg \
	file://usb.cfg \
	file://0001-Iris-dtsi-config-changes.patch \
	file://0002-Disable-Ethernet-MDIX.patch \
	file://0003-usbnet-fix-debugging-output-for-work-items.patch \
	file://0004-Go-back-to-old-mmc-numbering-scheme.patch \
	"

