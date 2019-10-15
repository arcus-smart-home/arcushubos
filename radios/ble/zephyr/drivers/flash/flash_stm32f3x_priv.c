/*
 * Copyright (c) 2016 RnDity Sp. z o.o.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <clock_control/stm32_clock_control.h>
#include <misc/__assert.h>
#include <string.h>

#include "flash_stm32f3x.h"

void flash_stm32_unlock(struct device *flash)
{
	const struct flash_stm32_dev_config *config = FLASH_CFG(flash);

	volatile struct stm32_flash *reg = FLASH_STRUCT(config->base);

	if ((reg->cr & FLASH_CR_LOCK) != 0) {
		/* Authorize the FLASH Registers access */
		reg->keyr = FLASH_KEY1;
		reg->keyr = FLASH_KEY2;
	}
}

void flash_stm32_lock(struct device *flash)
{
	const struct flash_stm32_dev_config *config = FLASH_CFG(flash);

	volatile struct stm32_flash *reg = FLASH_STRUCT(config->base);

	reg->cr |= FLASH_CR_LOCK;
}

u8_t flash_stm32_program_halfword(struct device *flash,
				     u32_t address,
				     u16_t data)
{
	u8_t status = FLASH_COMPLETE;

	const struct flash_stm32_dev_config *config = FLASH_CFG(flash);

	volatile struct stm32_flash *reg = FLASH_STRUCT(config->base);

	__ASSERT_NO_MSG(IS_FLASH_PROGRAM_ADDRESS(address));

	status = flash_stm32_wait_for_last_operation(flash,
			FLASH_ER_PRG_TIMEOUT);

	if (status == FLASH_COMPLETE) {
		reg->cr |= FLASH_CR_PG;

		*(volatile u16_t *)address = data;

		status = flash_stm32_wait_for_last_operation(flash,
				FLASH_ER_PRG_TIMEOUT);

		reg->cr &= ~FLASH_CR_PG;
	}

	return status;
}

u8_t flash_stm32_program_word(struct device *flash,
				 u32_t address,
				 u32_t data)
{
	u8_t status = FLASH_COMPLETE;

	const struct flash_stm32_dev_config *config = FLASH_CFG(flash);

	volatile struct stm32_flash *reg = FLASH_STRUCT(config->base);

	__ASSERT_NO_MSG(IS_FLASH_PROGRAM_ADDRESS(address));

	status = flash_stm32_wait_for_last_operation(flash,
			FLASH_ER_PRG_TIMEOUT);

	if (status == FLASH_COMPLETE) {
		reg->cr |= FLASH_CR_PG;

		*(volatile u16_t *)address = (u16_t)data;

		status = flash_stm32_wait_for_last_operation(flash,
				FLASH_ER_PRG_TIMEOUT);

		if (status == FLASH_COMPLETE) {
			address += 2;

			*(volatile u16_t *)address = data >> 16;

			status = flash_stm32_wait_for_last_operation(flash,
					FLASH_ER_PRG_TIMEOUT);
		}

		reg->cr &= ~FLASH_CR_PG;
	}

	return status;
}

u8_t flash_stm32_wait_for_last_operation(struct device *flash,
					    u32_t timeout)
{
	u8_t status = FLASH_COMPLETE;

	/* Check for the FLASH Status */
	status = flash_stm32_get_status(flash);

	/* Wait for a FLASH operation to complete or a TIMEOUT to occur. */
	while ((status == FLASH_BUSY) && (timeout != 0x00)) {
		status = flash_stm32_get_status(flash);
		timeout--;
	}

	if (timeout == 0x00) {
		status = FLASH_TIMEOUT;
	}

	return status;
}

u8_t flash_stm32_get_status(struct device *flash)
{
	u8_t status = FLASH_COMPLETE;

	const struct flash_stm32_dev_config *config = FLASH_CFG(flash);

	volatile struct stm32_flash *reg = FLASH_STRUCT(config->base);

	do {
		if ((reg->sr & FLASH_SR_BSY) == FLASH_SR_BSY) {
			status = FLASH_BUSY;
			break;
		}

		if ((reg->sr & FLASH_SR_WRPERR) != (u32_t)0x00) {
			status = FLASH_ERROR_WRITE_PROTECTION;
			break;
		}

		if ((reg->sr & FLASH_SR_PGERR) != (u32_t)0x00) {
			status = FLASH_ERROR_PROGRAM;
			break;
		}
	} while (0);

	return status;
}

u8_t flash_stm32_erase_page(struct device *flash,
			       u32_t page_address)
{
	u8_t status = FLASH_COMPLETE;

	const struct flash_stm32_dev_config *config = FLASH_CFG(flash);

	volatile struct stm32_flash *reg = FLASH_STRUCT(config->base);

	__ASSERT_NO_MSG(IS_FLASH_PROGRAM_ADDRESS(page_address));

	status = flash_stm32_wait_for_last_operation(flash,
			FLASH_ER_PRG_TIMEOUT);

	if (status == FLASH_COMPLETE) {
		reg->cr |= FLASH_CR_PER;
		reg->ar = page_address;
		reg->cr |= FLASH_CR_STRT;

		status = flash_stm32_wait_for_last_operation(flash,
				FLASH_ER_PRG_TIMEOUT);

		reg->cr &= ~FLASH_CR_PER;
	}

	return status;
}

u8_t flash_stm32_erase_all_pages(struct device *flash)
{
	u8_t status = FLASH_COMPLETE;

	const struct flash_stm32_dev_config *config = FLASH_CFG(flash);

	volatile struct stm32_flash *reg = FLASH_STRUCT(config->base);

	status = flash_stm32_wait_for_last_operation(flash,
			FLASH_ER_PRG_TIMEOUT);

	if (status == FLASH_COMPLETE) {
		reg->cr |= FLASH_CR_MER;
		reg->cr |= FLASH_CR_STRT;

		status = flash_stm32_wait_for_last_operation(flash,
				FLASH_ER_PRG_TIMEOUT);

		reg->cr &= ~FLASH_CR_MER;
	}

	return status;
}

void flash_stm32_read_data(void *data, u32_t address, size_t len)
{
	u8_t *addr = INT_TO_POINTER(address);

	memcpy(data, addr, len);
}
