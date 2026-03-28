/**
 * @file bpm_control.h
 * @brief Potentiometer ADC -> BPM and TIM6 step-clock update.
 */
#ifndef BPM_CONTROL_H
#define BPM_CONTROL_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

/* See AGENT/INTEGRATION_GUIDE.md */
#define BPM_MIN              60U
#define BPM_MAX              200U
#define BPM_DEFAULT          120U
/* TIM6: APB1 timer clock = 84 MHz (HCLK/4 * 2). PSC+1 = 8400 -> 10 kHz tick. */
#define TIM6_PSC_FOR_10KHZ   8399U
/* step_period_ms = 60000/BPM/2  =>  counts/step = 300000/BPM */
#define TIM6_ARR_FOR_BPM(bpm)  ((300000U / (uint32_t)(bpm)) - 1U)

#define ADC_READ_INTERVAL_MS    75U
#define BPM_UPDATE_HYSTERESIS     2U

void BpmControl_Init(TIM_HandleTypeDef *htim6, ADC_HandleTypeDef *hadc);
void BpmControl_ApplyBpm(uint16_t bpm);
uint16_t BpmControl_MapRawToBpm(uint32_t raw12);
uint32_t BpmControl_ReadPotRaw(void);
/** Call from main loop; polls ADC on a fixed interval and updates TIM6 when BPM changes. */
void BpmControl_Poll(void);
uint16_t BpmControl_GetLastAppliedBpm(void);

#endif /* BPM_CONTROL_H */
