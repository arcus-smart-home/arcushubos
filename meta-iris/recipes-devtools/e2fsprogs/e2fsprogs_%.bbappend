
# The fsck.ext* files are copies of e2fsck.  Same with mkfs.ext* files.
# Create symbolic links rather than copies of files.
do_install_append() {
	ln -sf e2fsck ${D}${base_sbindir}/fsck.ext2
	ln -sf e2fsck ${D}${base_sbindir}/fsck.ext3
	ln -sf e2fsck ${D}${base_sbindir}/fsck.ext4
	ln -sf e2fsck ${D}${base_sbindir}/fsck.ext4dev
	ln -sf mke2fs ${D}${base_sbindir}/mkfs.ext2
	ln -sf mke2fs ${D}${base_sbindir}/mkfs.ext3
	ln -sf mke2fs ${D}${base_sbindir}/mkfs.ext4
	ln -sf mke2fs ${D}${base_sbindir}/mkfs.ext4dev
}
