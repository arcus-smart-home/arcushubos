/*
 * Copyright (c) 2016 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <kernel.h>
#include <zephyr/types.h>
#include <errno.h>
#include <init.h>
#include <fs.h>
#include <misc/__assert.h>
#include <ff.h>

#define FATFS_MAX_FILE_NAME 12 /* Uses 8.3 SFN */

/* Memory pool for FatFs directory objects */
K_MEM_SLAB_DEFINE(fatfs_dirp_pool, sizeof(DIR),
			CONFIG_FS_FATFS_NUM_DIRS, 4);

/* Memory pool for FatFs file objects */
K_MEM_SLAB_DEFINE(fatfs_filep_pool, sizeof(FIL),
			CONFIG_FS_FATFS_NUM_FILES, 4);

static int translate_error(int error)
{
	switch (error) {
	case FR_OK:
		return 0;
	case FR_NO_FILE:
	case FR_NO_PATH:
	case FR_INVALID_NAME:
		return -ENOENT;
	case FR_DENIED:
		return -EACCES;
	case FR_EXIST:
		return -EEXIST;
	case FR_INVALID_OBJECT:
		return -EBADF;
	case FR_WRITE_PROTECTED:
		return -EROFS;
	case FR_INVALID_DRIVE:
	case FR_NOT_ENABLED:
	case FR_NO_FILESYSTEM:
		return -ENODEV;
	case FR_NOT_ENOUGH_CORE:
		return -ENOMEM;
	case FR_TOO_MANY_OPEN_FILES:
		return -EMFILE;
	case FR_INVALID_PARAMETER:
		return -EINVAL;
	case FR_LOCKED:
	case FR_TIMEOUT:
	case FR_MKFS_ABORTED:
	case FR_DISK_ERR:
	case FR_INT_ERR:
	case FR_NOT_READY:
		return -EIO;
	}

	return -EIO;
}

static int fatfs_open(struct fs_file_t *zfp, const char *file_name)
{
	FRESULT res;
	u8_t fs_mode;
	void *ptr;

	if (k_mem_slab_alloc(&fatfs_filep_pool, &ptr, K_NO_WAIT) == 0) {
		memset(ptr, 0, sizeof(FIL));
		zfp->filep = ptr;
	} else {
		return -ENOMEM;
	}

	fs_mode = FA_READ | FA_WRITE | FA_OPEN_ALWAYS;

	res = f_open(zfp->filep, &file_name[1], fs_mode);

	return translate_error(res);
}

static int fatfs_close(struct fs_file_t *zfp)
{
	FRESULT res;

	res = f_close(zfp->filep);

	/* Free file ptr memory */
	k_mem_slab_free(&fatfs_filep_pool, &zfp->filep);

	return translate_error(res);
}

static int fatfs_unlink(struct fs_mount_t *mountp, const char *path)
{
	FRESULT res;

	res = f_unlink(&path[1]);

	return translate_error(res);
}

static ssize_t fatfs_read(struct fs_file_t *zfp, void *ptr, size_t size)
{
	FRESULT res;
	unsigned int br;

	res = f_read(zfp->filep, ptr, size, &br);
	if (res != FR_OK) {
		return translate_error(res);
	}

	return br;
}

static ssize_t fatfs_write(struct fs_file_t *zfp, const void *ptr, size_t size)
{
	FRESULT res;
	unsigned int bw;

	res = f_write(zfp->filep, ptr, size, &bw);
	if (res != FR_OK) {
		return translate_error(res);
	}

	return bw;
}

static int fatfs_seek(struct fs_file_t *zfp, off_t offset, int whence)
{
	FRESULT res = FR_OK;
	off_t pos;

	switch (whence) {
	case FS_SEEK_SET:
		pos = offset;
		break;
	case FS_SEEK_CUR:
		pos = f_tell((FIL *)zfp->filep) + offset;
		break;
	case FS_SEEK_END:
		pos = f_size((FIL *)zfp->filep) + offset;
		break;
	default:
		return -EINVAL;
	}

	if ((pos < 0) || (pos > f_size((FIL *)zfp->filep))) {
		return -EINVAL;
	}

	res = f_lseek(zfp->filep, pos);

	return translate_error(res);
}

static off_t fatfs_tell(struct fs_file_t *zfp)
{
	return f_tell((FIL *)zfp->filep);
}

static int fatfs_truncate(struct fs_file_t *zfp, off_t length)
{
	FRESULT res = FR_OK;
	off_t cur_length = f_size((FIL *)zfp->filep);

	/* f_lseek expands file if new position is larger than file size */
	res = f_lseek(zfp->filep, length);
	if (res != FR_OK) {
		return translate_error(res);
	}

	if (length < cur_length) {
		res = f_truncate(zfp->filep);
	} else {
		/*
		 * Get actual length after expansion. This could be
		 * less if there was not enough space in the volume
		 * to expand to the requested length
		 */
		length = f_tell((FIL *)zfp->filep);

		res = f_lseek(zfp->filep, cur_length);
		if (res != FR_OK) {
			return translate_error(res);
		}

		/*
		 * The FS module does caching and optimization of
		 * writes. Here we write 1 byte at a time to avoid
		 * using additional code and memory for doing any
		 * optimization.
		 */
		unsigned int bw;
		u8_t c = 0;

		for (int i = cur_length; i < length; i++) {
			res = f_write(zfp->filep, &c, 1, &bw);
			if (res != FR_OK) {
				break;
			}
		}
	}

	return translate_error(res);
}

static int fatfs_sync(struct fs_file_t *zfp)
{
	FRESULT res = FR_OK;

	res = f_sync(zfp->filep);

	return translate_error(res);
}

static int fatfs_mkdir(struct fs_mount_t *mountp, const char *path)
{
	FRESULT res;

	res = f_mkdir(&path[1]);

	return translate_error(res);
}

static int fatfs_opendir(struct fs_dir_t *zdp, const char *path)
{
	FRESULT res;
	void *ptr;

	if (k_mem_slab_alloc(&fatfs_dirp_pool, &ptr, K_NO_WAIT) == 0) {
		memset(ptr, 0, sizeof(DIR));
		zdp->dirp = ptr;
	} else {
		return -ENOMEM;
	}


	res = f_opendir(zdp->dirp, &path[1]);

	return translate_error(res);
}

static int fatfs_readdir(struct fs_dir_t *zdp, struct fs_dirent *entry)
{
	FRESULT res;
	FILINFO fno;

	res = f_readdir(zdp->dirp, &fno);
	if (res == FR_OK) {
		entry->type = ((fno.fattrib & AM_DIR) ?
			       FS_DIR_ENTRY_DIR : FS_DIR_ENTRY_FILE);
		strcpy(entry->name, fno.fname);
		entry->size = fno.fsize;
	}

	return translate_error(res);
}

static int fatfs_closedir(struct fs_dir_t *zdp)
{
	FRESULT res;

	res = f_closedir(zdp->dirp);

	/* Free file ptr memory */
	k_mem_slab_free(&fatfs_dirp_pool, &zdp->dirp);

	return translate_error(res);
}

static int fatfs_stat(struct fs_mount_t *mountp,
		      const char *path, struct fs_dirent *entry)
{
	FRESULT res;
	FILINFO fno;

	res = f_stat(&path[1], &fno);
	if (res == FR_OK) {
		entry->type = ((fno.fattrib & AM_DIR) ?
			       FS_DIR_ENTRY_DIR : FS_DIR_ENTRY_FILE);
		strcpy(entry->name, fno.fname);
		entry->size = fno.fsize;
	}

	return translate_error(res);
}

static int fatfs_statvfs(struct fs_mount_t *mountp,
			 const char *path, struct fs_statvfs *stat)
{
	FATFS *fs;
	FRESULT res;

	res = f_getfree(&mountp->mnt_point[1], &stat->f_bfree, &fs);
	if (res != FR_OK) {
		return -EIO;
	}

	/*
	 * _MIN_SS holds the sector size. It is one of the configuration
	 * constants used by the FS module
	 */
	stat->f_bsize = _MIN_SS;
	stat->f_frsize = fs->csize * stat->f_bsize;
	stat->f_blocks = (fs->n_fatent - 2);

	return translate_error(res);
}

static int fatfs_mount(struct fs_mount_t *mountp)
{
	FRESULT res;

	res = f_mount((FATFS *)mountp->fs_data, &mountp->mnt_point[1], 1);

	/* If no file system found then create one */
	if (res == FR_NO_FILESYSTEM) {
		u8_t work[_MAX_SS];

		res = f_mkfs(&mountp->mnt_point[1],
				(FM_FAT | FM_SFD), 0, work, sizeof(work));
		if (res == FR_OK) {
			res = f_mount((FATFS *)mountp->fs_data,
					&mountp->mnt_point[1], 1);
		}
	}

	__ASSERT((res == FR_OK), "FS init failed (%d)", translate_error(res));

	return translate_error(res);

}

/* File system interface */
static struct fs_file_system_t fatfs_fs = {
	.open = fatfs_open,
	.close = fatfs_close,
	.read = fatfs_read,
	.write = fatfs_write,
	.lseek = fatfs_seek,
	.tell = fatfs_tell,
	.truncate = fatfs_truncate,
	.sync = fatfs_sync,
	.opendir = fatfs_opendir,
	.readdir = fatfs_readdir,
	.closedir = fatfs_closedir,
	.mount = fatfs_mount,
	.unlink = fatfs_unlink,
	.mkdir = fatfs_mkdir,
	.stat = fatfs_stat,
	.statvfs = fatfs_statvfs,
};

static int fatfs_init(struct device *dev)
{
	ARG_UNUSED(dev);

	return fs_register(FS_FATFS, &fatfs_fs);
}

SYS_INIT(fatfs_init, APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
