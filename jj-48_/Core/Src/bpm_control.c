/**
 * @file bpm_control.c
 */
#include "bpm_control.h"

static TIM_HandleTypeDef *s_htim6;
static ADC_HandleTypeDef *s_hadc;
static uint16_t s_last_applied_bpm;

void BpmControl_Init(TIM_HandleTypeDef *htim6, ADC_HandleTypeDef *hadc)
{
  s_htim6 = htim6;
  s_hadc = hadc;
  s_last_applied_bpm = 0U;
}

uint16_t BpmControl_GetLastAppliedBpm(void)
{
  return s_last_applied_bpm;
}

uint32_t BpmControl_ReadPotRaw(void)
{
  if (s_hadc == NULL) {
    return 0U;
  }
  /* Same sequence as Core/adc_reference/main.c (no HAL_ADC_Stop between reads). */
  if (HAL_ADC_Start(s_hadc) != HAL_OK) {
    return 0U;
  }
  if (HAL_ADC_PollForConversion(s_hadc, 100U) != HAL_OK) {
    return 0U;
  }
  return HAL_ADC_GetValue(s_hadc);
}

uint16_t BpmControl_MapRawToBpm(uint32_t raw12)
{
  if (raw12 > 4095U) {
    raw12 = 4095U;
  }
  return (uint16_t)(BPM_MIN + (raw12 * (BPM_MAX - BPM_MIN)) / 4095U);
}

void BpmControl_ApplyBpm(uint16_t bpm)
{
  if (s_htim6 == NULL) {
    return;
  }
  if (bpm < BPM_MIN) {
    bpm = BPM_MIN;
  }
  if (bpm > BPM_MAX) {
    bpm = BPM_MAX;
  }
  uint32_t arr = TIM6_ARR_FOR_BPM(bpm);
  if (arr > 0xFFFFU) {
    arr = 0xFFFFU;
  }
  __HAL_TIM_SET_AUTORELOAD(s_htim6, (uint16_t)arr);
  // Prevent the timer from overflowing and causing dropped beats when pot. hovers near a hysteresis edge.
  uint32_t cnt = __HAL_TIM_GET_COUNTER(s_htim6);
  if (cnt > arr) {
    __HAL_TIM_SET_COUNTER(s_htim6, 0U);
  }
  s_last_applied_bpm = bpm;
}

void BpmControl_Poll(void)
{
  static uint32_t s_adc_last_ms;

  uint32_t now_ms = HAL_GetTick();
  if (s_adc_last_ms == 0U) {
    s_adc_last_ms = now_ms;
    return;
  }
  if ((int32_t)(now_ms - s_adc_last_ms) < (int32_t)ADC_READ_INTERVAL_MS) {
    return;
  }
  s_adc_last_ms = now_ms;

  uint16_t bpm = BpmControl_MapRawToBpm(BpmControl_ReadPotRaw());
  int32_t db = (int32_t)bpm - (int32_t)s_last_applied_bpm;
  if (db < 0) {
    db = -db;
  }
  if (s_last_applied_bpm == 0U || (uint32_t)db >= BPM_UPDATE_HYSTERESIS) {
    BpmControl_ApplyBpm(bpm);
  }
}
