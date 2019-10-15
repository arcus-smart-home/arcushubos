/*
 * Copyright (c) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __POSIX_STAT_H__
#define __POSIX_STAT_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_PTHREAD_IPC
#include <kernel.h>

#ifdef CONFIG_POSIX_FS
#define S_IRWXU 00700
#define S_IRUSR 00400
#define S_IWUSR 00200
#define S_IXUSR 00100

#define S_IRWXG 00070
#define S_IRGRP 00040
#define S_IWGRP 00020
#define S_IXGRP 00010

#define S_IRWXO 00007
#define S_IROTH 00004
#define S_IWOTH 00002
#define S_IXOTH 00001

/* File open modes */
#define O_ACCMODE	   0003
#define O_RDONLY	     00
#define O_WRONLY	     01
#define O_RDWR		     02

#define SEEK_SET	0	/* Seek from beginning of file.  */
#define SEEK_CUR	1	/* Seek from current position.  */
#define SEEK_END	2	/* Seek from end of file.  */

struct stat {
	unsigned long st_size;
	unsigned long st_blksize;
	unsigned long st_blocks;
};

#endif	/* CONFIG_POSIX_FS */

#endif	/* CONFIG_PTHREAD_IPC */

#ifdef __cplusplus
}
#endif

#endif	/* __POSIX_STAT_H__ */
