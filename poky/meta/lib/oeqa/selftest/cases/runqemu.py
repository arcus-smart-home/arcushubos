#
# Copyright (c) 2017 Wind River Systems, Inc.
#

import re

from oeqa.selftest.case import OESelftestTestCase
from oeqa.utils.commands import bitbake, runqemu, get_bb_var
from oeqa.core.decorator.oeid import OETestID

class RunqemuTests(OESelftestTestCase):
    """Runqemu test class"""

    image_is_ready = False
    deploy_dir_image = ''
    # We only want to print runqemu stdout/stderr if there is a test case failure
    buffer = True

    def setUpLocal(self):
        super(RunqemuTests, self).setUpLocal()
        self.recipe = 'core-image-minimal'
        self.machine =  'qemux86-64'
        self.fstypes = "ext4 iso hddimg wic.vmdk wic.qcow2 wic.vdi"
        self.cmd_common = "runqemu nographic"

        self.write_config(
"""
MACHINE = "%s"
IMAGE_FSTYPES = "%s"
# 10 means 1 second
SYSLINUX_TIMEOUT = "10"
"""
% (self.machine, self.fstypes)
        )

        if not RunqemuTests.image_is_ready:
            RunqemuTests.deploy_dir_image = get_bb_var('DEPLOY_DIR_IMAGE')
            bitbake(self.recipe)
            RunqemuTests.image_is_ready = True

    @OETestID(2001)
    def test_boot_machine(self):
        """Test runqemu machine"""
        cmd = "%s %s" % (self.cmd_common, self.machine)
        with runqemu(self.recipe, ssh=False, launch_cmd=cmd) as qemu:
            self.assertTrue(qemu.runner.logged, "Failed: %s" % cmd)

    @OETestID(2002)
    def test_boot_machine_ext4(self):
        """Test runqemu machine ext4"""
        cmd = "%s %s ext4" % (self.cmd_common, self.machine)
        with runqemu(self.recipe, ssh=False, launch_cmd=cmd) as qemu:
            with open(qemu.qemurunnerlog) as f:
                self.assertTrue('rootfs.ext4' in f.read(), "Failed: %s" % cmd)

    @OETestID(2003)
    def test_boot_machine_iso(self):
        """Test runqemu machine iso"""
        cmd = "%s %s iso" % (self.cmd_common, self.machine)
        with runqemu(self.recipe, ssh=False, launch_cmd=cmd) as qemu:
            with open(qemu.qemurunnerlog) as f:
                self.assertTrue('media=cdrom' in f.read(), "Failed: %s" % cmd)

    @OETestID(2004)
    def test_boot_recipe_image(self):
        """Test runqemu recipe-image"""
        cmd = "%s %s" % (self.cmd_common, self.recipe)
        with runqemu(self.recipe, ssh=False, launch_cmd=cmd) as qemu:
            self.assertTrue(qemu.runner.logged, "Failed: %s" % cmd)

    @OETestID(2005)
    def test_boot_recipe_image_vmdk(self):
        """Test runqemu recipe-image vmdk"""
        cmd = "%s %s wic.vmdk" % (self.cmd_common, self.recipe)
        with runqemu(self.recipe, ssh=False, launch_cmd=cmd) as qemu:
            with open(qemu.qemurunnerlog) as f:
                self.assertTrue('format=vmdk' in f.read(), "Failed: %s" % cmd)

    @OETestID(2006)
    def test_boot_recipe_image_vdi(self):
        """Test runqemu recipe-image vdi"""
        cmd = "%s %s wic.vdi" % (self.cmd_common, self.recipe)
        with runqemu(self.recipe, ssh=False, launch_cmd=cmd) as qemu:
            with open(qemu.qemurunnerlog) as f:
                self.assertTrue('format=vdi' in f.read(), "Failed: %s" % cmd)

    @OETestID(2007)
    def test_boot_deploy(self):
        """Test runqemu deploy_dir_image"""
        cmd = "%s %s" % (self.cmd_common, self.deploy_dir_image)
        with runqemu(self.recipe, ssh=False, launch_cmd=cmd) as qemu:
            self.assertTrue(qemu.runner.logged, "Failed: %s" % cmd)

    @OETestID(2008)
    def test_boot_deploy_hddimg(self):
        """Test runqemu deploy_dir_image hddimg"""
        cmd = "%s %s hddimg" % (self.cmd_common, self.deploy_dir_image)
        with runqemu(self.recipe, ssh=False, launch_cmd=cmd) as qemu:
            with open(qemu.qemurunnerlog) as f:
                self.assertTrue(re.search('file=.*.hddimg', f.read()), "Failed: %s" % cmd)

    @OETestID(2009)
    def test_boot_machine_slirp(self):
        """Test runqemu machine slirp"""
        cmd = "%s slirp %s" % (self.cmd_common, self.machine)
        with runqemu(self.recipe, ssh=False, launch_cmd=cmd) as qemu:
            with open(qemu.qemurunnerlog) as f:
                self.assertTrue(' -netdev user' in f.read(), "Failed: %s" % cmd)

    @OETestID(2009)
    def test_boot_machine_slirp_qcow2(self):
        """Test runqemu machine slirp qcow2"""
        cmd = "%s slirp wic.qcow2 %s" % (self.cmd_common, self.machine)
        with runqemu(self.recipe, ssh=False, launch_cmd=cmd) as qemu:
            with open(qemu.qemurunnerlog) as f:
                self.assertTrue('format=qcow2' in f.read(), "Failed: %s" % cmd)

    @OETestID(2010)
    def test_boot_qemu_boot(self):
        """Test runqemu /path/to/image.qemuboot.conf"""
        qemuboot_conf = "%s-%s.qemuboot.conf" % (self.recipe, self.machine)
        qemuboot_conf = os.path.join(self.deploy_dir_image, qemuboot_conf)
        if not os.path.exists(qemuboot_conf):
            self.skipTest("%s not found" % qemuboot_conf)
        cmd = "%s %s" % (self.cmd_common, qemuboot_conf)
        with runqemu(self.recipe, ssh=False, launch_cmd=cmd) as qemu:
            self.assertTrue(qemu.runner.logged, "Failed: %s" % cmd)

    @OETestID(2011)
    def test_boot_rootfs(self):
        """Test runqemu /path/to/rootfs.ext4"""
        rootfs = "%s-%s.ext4" % (self.recipe, self.machine)
        rootfs = os.path.join(self.deploy_dir_image, rootfs)
        if not os.path.exists(rootfs):
            self.skipTest("%s not found" % rootfs)
        cmd = "%s %s" % (self.cmd_common, rootfs)
        with runqemu(self.recipe, ssh=False, launch_cmd=cmd) as qemu:
            self.assertTrue(qemu.runner.logged, "Failed: %s" % cmd)
