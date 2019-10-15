/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief ARM Core CMSE API
 *
 * CMSE API for Cortex-M23/M33 CPUs.
 */

#ifndef _ARM_CORTEXM_CMSE__H_
#define _ARM_CORTEXM_CMSE__H_

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _ASMLANGUAGE

/* nothing */

#else

#include <arm_cmse.h>
#include <stdint.h>

/*
 * Address information retrieval based on the TT instructions.
 *
 * The TT instructions are used to check the access permissions that different
 * security states and privilege levels have on memory at a specified address
 */

/**
 * @brief Get the MPU region number of an address
 *
 * Get the MPU region that the address maps to. Return non-zero
 * to indicate that a valid MPU region was retrieved, and store the
 * MPU region information in the supplied location.
 *
 * Note:
 * Obtained region is valid only if:
 * - the function is called from privileged mode
 * - the MPU is implemented and enabled
 * - the given address matches a single, enabled MPU region
 *
 * @param addr The address for which the MPU region is requested
 * @param p_region Output pointer to the location to store the MPU region
 *
 * @return non-zero if @ref region contains a valid MPU region, otherwise 0.
  */
int arm_cmse_mpu_region_get(u32_t addr, u8_t *p_region);

/**
 * @brief Read accessibility of an address
 *
 * Evaluates whether a specified memory location can be read according to the
 * permissions of the current state MPU and the specified operation mode.
 *
 * This function shall always return zero:
 * - if executed from an unprivileged mode,
 * - if the address matches multiple MPU regions.
 *
 * @param addr The address for which the readability is requested
 * @param force_npriv Instruct to return the readability of the address
 *                    for unprivileged access, regardless of whether the current
 *                    mode is privileged or unprivileged.
 *
 * @return 1 if address is readable, 0 otherwise.
 */
int arm_cmse_addr_read_ok(u32_t addr, int force_npriv);

/**
 * @brief Read and Write accessibility of an address
 *
 * Evaluates whether a specified memory location can be read/written according
 * to the permissions of the current state MPU and the specified operation
 * mode.
 *
 * This function shall always return zero:
 * - if executed from an unprivileged mode,
 * - if the address matches multiple MPU regions.
 *
 * @param addr The address for which the RW ability is requested
 * @param force_npriv Instruct to return the RW ability of the address
 *                    for unprivileged access, regardless of whether the current
 *                    mode is privileged or unprivileged.
 *
 * @return 1 if address is Read and Writable, 0 otherwise.
 */
int arm_cmse_addr_readwrite_ok(u32_t addr, int force_npriv);

/**
 * @brief Read accessibility of an address range
 *
 * Evaluates whether a memory address range, specified by its base address
 * and size, can be read according to the permissions of the current state MPU
 * and the specified operation mode.
 *
 * This function shall always return zero:
 * - if executed from an unprivileged mode,
 * - if the address range overlaps with multiple MPU (and/or SAU/IDAU) regions.
 *
 * @param addr The base address of an address range,
 *             for which the readability is requested
 * @param size The size of the address range
 * @param force_npriv Instruct to return the readability of the address range
 *                    for unprivileged access, regardless of whether the current
 *                    mode is privileged or unprivileged.
 *
 * @return 1 if address range is readable, 0 otherwise.
 */
int arm_cmse_addr_range_read_ok(u32_t addr, u32_t size, int force_npriv);

/**
 * @brief Read and Write accessibility of an address range
 *
 * Evaluates whether a memory address range, specified by its base address
 * and size, can be read/written according to the permissions of the current
 * state MPU and the specified operation mode.
 *
 * This function shall always return zero:
 * - if executed from an unprivileged mode,
 * - if the address range overlaps with multiple MPU (and/or SAU/IDAU) regions.
 *
 * @param addr The base address of an address range,
 *             for which the RW ability is requested
 * @param size The size of the address range
 * @param force_npriv Instruct to return the RW ability of the address range
 *                    for unprivileged access, regardless of whether the current
 *                    mode is privileged or unprivileged.
 *
 * @return 1 if address range is Read and Writable, 0 otherwise.
 */
int arm_cmse_addr_range_readwrite_ok(u32_t addr, u32_t size, int force_npriv);

/* Required for C99 compilation */
#ifndef typeof
#define typeof  __typeof__
#endif

/**
 * @brief Read accessibility of an object
 *
 * Evaluates whether a given object can be read according to the
 * permissions of the current state MPU.
 *
 * The macro shall always evaluate to zero if called from an unprivileged mode.
 *
 * @param p_obj Pointer to the given object
 *              for which the readability is requested
 *
 * @pre Object is allocated in a single MPU (and/or SAU/IDAU) region.
 *
 * @return p_obj if object is readable, NULL otherwise.
 */
#define ARM_CMSE_OBJECT_READ_OK(p_obj) \
	cmse_check_pointed_object(p_obj, CMSE_MPU_READ)

/**
 * @brief Read accessibility of an object (nPRIV mode)
 *
 * Evaluates whether a given object can be read according to the
 * permissions of the current state MPU (unprivileged read).
 *
 * The macro shall always evaluate to zero if called from an unprivileged mode.
 *
 * @param p_obj Pointer to the given object
 *              for which the readability is requested
 *
 * @pre Object is allocated in a single MPU (and/or SAU/IDAU) region.
 *
 * @return p_obj if object is readable, NULL otherwise.
 */
#define ARM_CMSE_OBJECT_UNPRIV_READ_OK(p_obj) \
	cmse_check_pointed_object(p_obj, CMSE_MPU_UNPRIV | CMSE_MPU_READ)

/**
 * @brief Read and Write accessibility of an object
 *
 * Evaluates whether a given object can be read and written
 * according to the permissions of the current state MPU.
 *
 * The macro shall always evaluate to zero if called from an unprivileged mode.
 *
 * @param p_obj Pointer to the given object
 *              for which the read and write ability is requested
 *
 * @pre Object is allocated in a single MPU (and/or SAU/IDAU) region.
 *
 * @return p_obj if object is Read and Writable, NULL otherwise.
 */
#define ARM_CMSE_OBJECT_READWRITE_OK(p_obj) \
	cmse_check_pointed_object(p_obj, CMSE_MPU_READWRITE)

/**
 * @brief Read and Write accessibility of an object (nPRIV mode)
 *
 * Evaluates whether a given object can be read and written according
 * to the permissions of the current state MPU (unprivileged read/write).
 *
 * The macro shall always evaluate to zero if called from an unprivileged mode.
 *
 * @param p_obj Pointer to the given object
 *              for which the read and write ability is requested
 *
 * @pre Object is allocated in a single MPU (and/or SAU/IDAU) region.
 *
 * @return p_obj if object is Read and Writable, NULL otherwise.
 */
#define ARM_CMSE_OBJECT_UNPRIV_READWRITE_OK(p_obj) \
	cmse_check_pointed_object(p_obj, CMSE_MPU_UNPRIV | CMSE_MPU_READWRITE)

#endif /* _ASMLANGUAGE */

#ifdef __cplusplus
}
#endif

#endif /* _ARM_CORTEXM_CMSE__H_ */
