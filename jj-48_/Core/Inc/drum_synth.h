/**
  ******************************************************************************
  * @file    drum_synth.h
  * @brief   Snare and hi-hat waveform generation with ADSR for DAC output.
  *          Milestone 1 - John's part: verify on scope at PA4 (DAC_OUT1).
  ******************************************************************************
  */

#ifndef DRUM_SYNTH_H
#define DRUM_SYNTH_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/** Drum types for Milestone 1 */
typedef enum {
  DRUM_SNARE = 0,
  DRUM_HIHAT = 1,
  DRUM_COUNT
} DrumType_t;

/** ADSR envelope parameters (times in samples at SAMPLE_RATE) */
typedef struct {
  uint32_t attack_samples;
  uint32_t decay_samples;
  uint32_t sustain_level;   /* 0..4095 */
  uint32_t release_samples;
} ADSR_Params_t;

/** Sample rate for DAC output (Hz). TIM6 must be configured to this rate. */
#define DRUM_SAMPLE_RATE_HZ  16000U

/**
  * @brief  Initialize drum synth (noise seed, envelope state).
  */
void DrumSynth_Init(void);

/**
  * @brief  Trigger a drum hit. Envelope runs until release finishes.
  * @param  drum  DRUM_SNARE or DRUM_HIHAT
  */
void DrumSynth_Trigger(DrumType_t drum);

/**
  * @brief  Get next 12-bit sample (0..4095) for DAC. Call from TIM6 update IRQ.
  * @retval DAC value; 2048 when idle (mid-scale for minimal DC).
  */
uint32_t DrumSynth_GetNextSample(void);

#ifdef __cplusplus
}
#endif

#endif /* DRUM_SYNTH_H */
