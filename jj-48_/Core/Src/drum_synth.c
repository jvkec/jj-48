/**
  ******************************************************************************
  * @file    drum_synth.c
  * @brief   Snare and hi-hat with ADSR. Noise-based waveforms for scope verification.
  ******************************************************************************
  */

#include "drum_synth.h"
#include <string.h>

/* Private defines: envelope times at 16 kHz */
#define SNARE_DECAY_MS    80U
#define HIHAT_DECAY_MS    30U
#define SAMPLES_PER_MS    (DRUM_SAMPLE_RATE_HZ / 1000U)
#define SNARE_DECAY_SAMPLES  (SNARE_DECAY_MS * SAMPLES_PER_MS)
#define HIHAT_DECAY_SAMPLES  (HIHAT_DECAY_MS * SAMPLES_PER_MS)

/* DAC is 12-bit; use 2048 as silence (mid-scale) to avoid large DC offset when idle */
#define DAC_MID           2048U
#define DAC_MAX           4095U

/* Envelope phase */
typedef enum {
  ENV_IDLE,
  ENV_ATTACK,
  ENV_DECAY,
  ENV_SUSTAIN,
  ENV_RELEASE
} EnvPhase_t;

/* Per-drum state */
typedef struct {
  EnvPhase_t    phase;
  uint32_t      phase_sample;
  uint32_t      gain;           /* 0..4095 */
  ADSR_Params_t adsr;
} DrumState_t;

/* 16-bit LFSR for noise (max period 2^16-1) */
static uint16_t lfsr = 0xACE1U;
static DrumState_t drum_state[DRUM_COUNT];

static uint16_t Noise_Next(void)
{
  uint16_t bit = (lfsr ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5)) & 1U;
  lfsr = (lfsr >> 1) | (bit << 15);
  return lfsr;
}

static void Envelope_Step(DrumState_t *s)
{
  switch (s->phase) {
    case ENV_IDLE:
      return;
    case ENV_ATTACK:
      s->phase_sample++;
      if (s->phase_sample >= s->adsr.attack_samples) {
        s->phase_sample = 0;
        s->phase = ENV_DECAY;
      } else {
        s->gain = (s->phase_sample * DAC_MAX) / (s->adsr.attack_samples + 1);
      }
      break;
    case ENV_DECAY:
      s->phase_sample++;
      if (s->phase_sample >= s->adsr.decay_samples) {
        s->phase_sample = 0;
        s->phase = (s->adsr.sustain_level > 0) ? ENV_SUSTAIN : ENV_RELEASE;
        s->gain = s->adsr.sustain_level;
      } else {
        /* linear decay from 4095 to sustain */
        s->gain = DAC_MAX - (s->phase_sample * (DAC_MAX - s->adsr.sustain_level)) / (s->adsr.decay_samples + 1);
      }
      break;
    case ENV_SUSTAIN:
      s->phase_sample++;
      if (s->adsr.release_samples == 0) {
        s->phase = ENV_IDLE;
        s->gain = 0;
      } else {
        s->phase = ENV_RELEASE;
        s->phase_sample = 0;
      }
      break;
    case ENV_RELEASE:
      s->phase_sample++;
      if (s->phase_sample >= s->adsr.release_samples) {
        s->phase = ENV_IDLE;
        s->gain = 0;
      } else {
        s->gain = s->adsr.sustain_level - (s->phase_sample * s->adsr.sustain_level) / (s->adsr.release_samples + 1);
      }
      break;
  }
}

void DrumSynth_Init(void)
{
  memset(drum_state, 0, sizeof(drum_state));
  lfsr = 0xACE1U;

  /* Snare: instant attack, short decay, no sustain/release */
  drum_state[DRUM_SNARE].adsr.attack_samples  = 0;
  drum_state[DRUM_SNARE].adsr.decay_samples   = SNARE_DECAY_SAMPLES;
  drum_state[DRUM_SNARE].adsr.sustain_level   = 0;
  drum_state[DRUM_SNARE].adsr.release_samples = 0;
  drum_state[DRUM_SNARE].phase = ENV_IDLE;
  drum_state[DRUM_SNARE].gain  = 0;

  /* Hi-hat: instant attack, very short decay */
  drum_state[DRUM_HIHAT].adsr.attack_samples  = 0;
  drum_state[DRUM_HIHAT].adsr.decay_samples   = HIHAT_DECAY_SAMPLES;
  drum_state[DRUM_HIHAT].adsr.sustain_level   = 0;
  drum_state[DRUM_HIHAT].adsr.release_samples = 0;
  drum_state[DRUM_HIHAT].phase = ENV_IDLE;
  drum_state[DRUM_HIHAT].gain  = 0;
}

void DrumSynth_Trigger(DrumType_t drum)
{
  if (drum >= DRUM_COUNT) return;
  DrumState_t *s = &drum_state[drum];
  s->phase = (s->adsr.attack_samples > 0) ? ENV_ATTACK : ENV_DECAY;
  s->phase_sample = 0;
  s->gain = (s->adsr.attack_samples > 0) ? 0 : DAC_MAX;
}

uint32_t DrumSynth_GetNextSample(void)
{
  uint32_t out = 0;
  for (int i = 0; i < DRUM_COUNT; i++) {
    DrumState_t *s = &drum_state[i];
    Envelope_Step(s);
    if (s->phase != ENV_IDLE && s->gain > 0) {
      uint16_t n = Noise_Next();
      /* Scale noise 0..4095 and apply envelope; mix with other drums */
      uint32_t sample = (s->gain * (n & 0xFFFU)) >> 12;
      out += sample;
    }
  }
  if (out > DAC_MAX) out = DAC_MAX;
  /* When any drum is active, output 0..4095; when idle use mid-scale to reduce DC */
  if (out == 0) {
    return DAC_MID;
  }
  return out;
}
