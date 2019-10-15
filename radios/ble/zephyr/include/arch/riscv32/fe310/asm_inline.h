/*
 * Copyright (c) 2017 Jean-Paul Etienne <fractalclone@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _ASM_INLINE_PUBLIC_H
#define _ASM_INLINE_PUBLIC_H

/*
 * The file must not be included directly
 * Include arch/cpu.h instead
 */

#if defined(__GNUC__)
#include <arch/riscv32/fe310/asm_inline_gcc.h>
#else
#error "Supports only GNU C compiler"
#endif

#endif /* _ASM_INLINE_PUBLIC_H */
