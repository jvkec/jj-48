/**
  ******************************************************************************
  * @file    drum_synth.h
  * @brief   4-voice drum synthesizer for I2S output via MAX98357.
  *          Kick, snare, hi-hat, clap with ADSR/exponential envelopes.
  ******************************************************************************
  */

#ifndef DRUM_SYNTH_H
#define DRUM_SYNTH_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef enum {
  DRUM_KICK  = 0,
  DRUM_SNARE = 1,
  DRUM_HIHAT = 2,
  DRUM_CLAP  = 3,
  DRUM_COUNT
} DrumType_t;

/** Sample rate shared with I2S peripheral — TIM6 or I2S clock must match. */
#define DRUM_SAMPLE_RATE_HZ  16000U

/**
  * @brief  Initialise all voice state and noise seed.  Call once at startup.
  */
void DrumSynth_Init(void);

/**
  * @brief  Trigger a drum hit.  Safe to re-trigger while a voice is active.
  * @param  drum  One of DRUM_KICK, DRUM_SNARE, DRUM_HIHAT, DRUM_CLAP.
  */
void DrumSynth_Trigger(DrumType_t drum);

/**
  * @brief  Return the next mixed signed 16-bit sample for I2S.
  *         Call once per sample period from the DMA half/complete callback.
  * @retval Signed 16-bit PCM value ready for I2S (Philips, 16-bit).
  */
int16_t DrumSynth_GetNextSample(void);

#ifdef __cplusplus
}
#endif

#endif /* DRUM_SYNTH_H */
