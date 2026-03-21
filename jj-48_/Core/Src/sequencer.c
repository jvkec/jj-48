/**
 * @file sequencer.c
 */
#include "sequencer.h"
#include "drum_synth.h"
#include "stm32f4xx_hal.h"

volatile uint8_t pattern[DRUM_COUNT][SEQUENCER_NUM_STEPS];
volatile uint8_t current_step;

void Sequencer_InitDemoPattern(void)
{
  for (uint32_t t = 0U; t < (uint32_t)DRUM_COUNT; t++) {
    for (uint32_t s = 0U; s < SEQUENCER_NUM_STEPS; s++) {
      pattern[t][s] = 0U;
    }
  }
  pattern[DRUM_KICK][0]  = 1U;
  pattern[DRUM_KICK][4]  = 1U;
  pattern[DRUM_SNARE][2] = 1U;
  pattern[DRUM_SNARE][6] = 1U;
  for (uint32_t s = 0U; s < SEQUENCER_NUM_STEPS; s++) {
    pattern[DRUM_HIHAT][s] = 1U;
  }
  current_step = 0U;
}

static void Sequencer_OnStep(void)
{
  uint8_t step = current_step;
  for (uint32_t t = 0U; t < (uint32_t)DRUM_COUNT; t++) {
    if (pattern[t][step]) {
      DrumSynth_Trigger((DrumType_t)t);
    }
  }
  current_step = (uint8_t)((step + 1U) % SEQUENCER_NUM_STEPS);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance != TIM6) {
    return;
  }
  Sequencer_OnStep();
}
