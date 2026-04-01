#pragma once

#include "stm32f4xx_hal.h"
#include "drum_synth.h"
#include "sequencer.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Kept identical names to original main.h for drop-in use */
#define FLASH_USER_START_ADDR  (0x08060000UL) /* sector 7 */
#define FLASH_USER_END_ADDR    (0x0807FFFFUL)

typedef struct {
  uint8_t pattern[DRUM_COUNT][SEQUENCER_NUM_STEPS];
  uint32_t valid;
} SaveFlashData;

HAL_StatusTypeDef flash_write_bytes(uint32_t flash_addr, uint32_t *data, uint32_t word_count);
HAL_StatusTypeDef flash_erase_sector_7(void);
HAL_StatusTypeDef flash_read_data(SaveFlashData *data);

#ifdef __cplusplus
}
#endif

