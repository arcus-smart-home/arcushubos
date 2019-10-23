FILESEXTRAPATHS_append := "${THISDIR}/linux-fslc"

COMPATIBLE_MACHINE = "(imxdimagic)"

# Clear out the kernel extra features to make sure netfilter support doesn't
#  get added back in!
#KERNEL_EXTRA_FEATURES = ""
#KERNEL_FEATURES_append = ""

# Update to 4.14.22
SRCREV = "fe88b0b06a4a31bf9477bfc219229a715eda4be7"

KERNEL_DEVICETREE = " \
    imx6q-mq.dtb \
    imx6q-imagic.dtb \
    imx6dl-imagic.dtb \
    imx6q-mq-ldo.dtb \
 "

SRC_URI += " \
	file://defconfig \
	file://0001-Added-Quectel-mods-to-support-EC25-A-module-in-QCI-W.patch \
	file://0002-imx6qmq-board-support.patch \
	file://0003-imx6qmq-device-tree-additions.patch \
	file://0004-imx6qmq-sound-driver-modifications.patch \
	file://0005-IRIS-modifications-and-cleanup.patch \
	file://0006-Up-IMX-watchdog-default-timeout.patch \
	file://0007-IMX-D-IMAGIC-board-support.patch \
	file://0008-IMX-D-IMAGIC-driver-support.patch \
	file://0009-IMX-D-IMAGIC-sound-support.patch \
	file://0010-Add-ZTE-LTE-ME3630-module-support.patch \
	file://0011-IMX-D-IMAGIC-CUT-2-modifications.patch \
	file://0012-Add-BT-RST-signal.patch \
	file://0013-Set-LED-ring-to-purple-at-boot.patch \
	"

