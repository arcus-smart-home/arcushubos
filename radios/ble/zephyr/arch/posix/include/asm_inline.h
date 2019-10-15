/* Inline assembler kernel functions and macros */

/*
 * Copyright (c) 2015, Wind River Systems, Inc.
 * Copyright (c) 2017, Oticon A/S
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _ASM_INLINE_H
#define _ASM_INLINE_H

#if !defined(CONFIG_ARCH_POSIX)
#error The arch/posix/include/asm_inline.h is only for the POSIX architecture
#endif

#if defined(__GNUC__)
#include <asm_inline_gcc.h> /* The empty one.. */
#include <arch/posix/asm_inline_gcc.h>
#else
#include <asm_inline_other.h>
#endif /* __GNUC__ */

#endif /* _ASM_INLINE_H */
