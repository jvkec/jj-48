#include "flash_storage.h"

#include <string.h>
#include "stm32f4xx_hal_flash_ex.h"
#include "stm32f446xx.h"

HAL_StatusTypeDef flash_write_bytes(uint32_t flash_addr, uint32_t *data, uint32_t word_count)
{
  HAL_StatusTypeDef ret = HAL_OK;

  HAL_FLASH_Unlock();
  for (uint32_t i = 0; i < word_count; i++) {
    ret = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, flash_addr + (i * 4U), data[i]);
    if (ret != HAL_OK) {
      break;
    }
  }
  HAL_FLASH_Lock();

  return ret;
}

HAL_StatusTypeDef flash_erase_sector_7(void)
{
  HAL_StatusTypeDef ret = HAL_OK;
  FLASH_EraseInitTypeDef erase_init;
  uint32_t sector_error = 0U;

  HAL_FLASH_Unlock();
  erase_init.TypeErase = FLASH_TYPEERASE_SECTORS;
  erase_init.VoltageRange = FLASH_VOLTAGE_RANGE_3;
  erase_init.Sector = FLASH_SECTOR_7;
  erase_init.NbSectors = 1U;

  ret = HAL_FLASHEx_Erase(&erase_init, &sector_error);
  HAL_FLASH_Lock();

  return ret;
}

HAL_StatusTypeDef flash_read_data(SaveFlashData *data)
{
  if (data == NULL) {
    return HAL_ERROR;
  }

  memcpy(data, (const void *)FLASH_USER_START_ADDR, sizeof(SaveFlashData));

  /* kept identical behavior: require 0xDEADBEEF marker */
  if (data->valid != 0xDEADBEEFUL) {
    return HAL_ERROR;
  }

  return HAL_OK;
}

