/*
 * Copyright (c) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef _POSIX_SEMAPHORE_H
#define _POSIX_SEMAPHORE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <posix/time.h>
#include "sys/types.h"

int sem_destroy(sem_t *semaphore);
int sem_getvalue(sem_t *restrict semaphore, int *restrict value);
int sem_init(sem_t *semaphore, int pshared, unsigned int value);
int sem_post(sem_t *semaphore);
int sem_timedwait(sem_t *restrict semaphore, struct timespec *restrict abstime);
int sem_trywait(sem_t *semaphore);
int sem_wait(sem_t *semaphore);

#ifdef __cplusplus
}
#endif

#endif /* POSIX_SEMAPHORE_H */
