/**
  ******************************************************************************
  * @file    drum_synth.c
  * @brief   4-voice drum synthesizer — kick, snare, hi-hat, clap.
  *
  *  Waveform recipes:
  *    Kick  — sine oscillator with exponential pitch sweep 200→45 Hz,
  *            long amplitude decay (128 ms τ).  Classic 808-style thump.
  *    Snare — sine tone at 185 Hz (fast decay 16 ms) mixed with LFSR
  *            noise (slower decay 64 ms).  Tone gives body, noise gives snap.
  *    Hi-hat— LFSR noise through a 1-pole HPF (fc ≈ 3 kHz), short decay
  *            (32 ms τ) for a metallic closed-hat character.
  *    Clap  — 3 short noise bursts (2 ms on, 3 ms gap) followed by an
  *            exponentially decaying noise tail (128 ms τ).
  ******************************************************************************
  */

#include "drum_synth.h"
#include <string.h>

/* ===== Sine LUT (256 entries, Fixed Point Q15, +/-32767) ============== */
static const int16_t sine_tab[256] = {
        0,    804,   1608,   2410,   3212,   4011,   4808,   5602,
     6393,   7179,   7962,   8739,   9512,  10278,  11039,  11793,
    12539,  13279,  14010,  14732,  15446,  16151,  16846,  17530,
    18204,  18868,  19519,  20159,  20787,  21403,  22005,  22594,
    23170,  23731,  24279,  24811,  25329,  25832,  26319,  26790,
    27245,  27683,  28105,  28510,  28898,  29268,  29621,  29956,
    30273,  30571,  30852,  31113,  31356,  31580,  31785,  31971,
    32137,  32285,  32412,  32521,  32609,  32678,  32728,  32757,
    32767,  32757,  32728,  32678,  32609,  32521,  32412,  32285,
    32137,  31971,  31785,  31580,  31356,  31113,  30852,  30571,
    30273,  29956,  29621,  29268,  28898,  28510,  28105,  27683,
    27245,  26790,  26319,  25832,  25329,  24811,  24279,  23731,
    23170,  22594,  22005,  21403,  20787,  20159,  19519,  18868,
    18204,  17530,  16846,  16151,  15446,  14732,  14010,  13279,
    12539,  11793,  11039,  10278,   9512,   8739,   7962,   7179,
     6393,   5602,   4808,   4011,   3212,   2410,   1608,    804,
        0,   -804,  -1608,  -2410,  -3212,  -4011,  -4808,  -5602,
    -6393,  -7179,  -7962,  -8739,  -9512, -10278, -11039, -11793,
   -12539, -13279, -14010, -14732, -15446, -16151, -16846, -17530,
   -18204, -18868, -19519, -20159, -20787, -21403, -22005, -22594,
   -23170, -23731, -24279, -24811, -25329, -25832, -26319, -26790,
   -27245, -27683, -28105, -28510, -28898, -29268, -29621, -29956,
   -30273, -30571, -30852, -31113, -31356, -31580, -31785, -31971,
   -32137, -32285, -32412, -32521, -32609, -32678, -32728, -32757,
   -32767, -32757, -32728, -32678, -32609, -32521, -32412, -32285,
   -32137, -31971, -31785, -31580, -31356, -31113, -30852, -30571,
   -30273, -29956, -29621, -29268, -28898, -28510, -28105, -27683,
   -27245, -26790, -26319, -25832, -25329, -24811, -24279, -23731,
   -23170, -22594, -22005, -21403, -20787, -20159, -19519, -18868,
   -18204, -17530, -16846, -16151, -15446, -14732, -14010, -13279,
   -12539, -11793, -11039, -10278,  -9512,  -8739,  -7962,  -7179,
    -6393,  -5602,  -4808,  -4011,  -3212,  -2410,  -1608,   -804,
};

/* ===== 16-bit maximal-length LFSR (period 2^16-1) ======================== */
static uint16_t lfsr = 0xACE1U;

static inline int16_t Noise_Next(void)
{
  /* x^16 + x^14 + x^13 + x^11 + 1  (taps 0,2,3,5 in Fibonacci form) */
  uint16_t bit = (lfsr ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5)) & 1U;
  lfsr = (lfsr >> 1) | (bit << 15);
  return (int16_t)lfsr;   /* reinterpret as signed — uniform over ±32 k */
}

/* ===== Phase-accumulator helpers ========================================= */
/*
 * Phase accumulator is Q16.16.  Top 8 bits index into the 256-entry table.
 *   phase_inc = freq_hz * 256 * 65536 / 16000
 *             = freq_hz * 131072 / 125           (pure integer, no float)
 */
#define FREQ_TO_INC(hz)  ((uint32_t)((uint32_t)(hz) * 131072U / 125U))

/* ===== Exponential-decay rates (Q16 per-sample multiplier) =============== */
/*
 * env[n+1] = env[n] * RATE >> 16
 * Time constant τ ≈ 65536 / (65536 − RATE) samples.
 * At 16 kHz: τ_ms ≈ τ_samples / 16.
 */
#define DECAY_TAU_256   65280U   /* τ = 256 samp =  16 ms */
#define DECAY_TAU_512   65408U   /* τ = 512 samp =  32 ms */
#define DECAY_TAU_1024  65472U   /* τ = 1024 samp = 64 ms */
#define DECAY_TAU_2048  65504U   /* τ = 2048 samp = 128 ms */

/* ===== Per-instrument tuning ============================================= */

/* Kick — sine + pitch sweep */
#define KICK_FREQ_START     FREQ_TO_INC(200)
#define KICK_FREQ_END       FREQ_TO_INC(45)
#define KICK_PITCH_SHIFT    9U          /* pitch sweep τ = 512 samp (32 ms) */
#define KICK_AMP_DECAY      DECAY_TAU_2048

/* Snare — sine body + noise snap */
#define SNARE_TONE_FREQ     FREQ_TO_INC(185)
#define SNARE_TONE_DECAY    DECAY_TAU_256
#define SNARE_NOISE_DECAY   DECAY_TAU_1024

/* Hi-hat — HPF noise */
#define HIHAT_AMP_DECAY     DECAY_TAU_512
#define HIHAT_HPF_ALPHA     17726U  /* Q15 LPF coeff for HPF-via-subtraction,
                                       fc ≈ 3 kHz at 16 kHz Fs */

/* Clap — burst noise + tail */
#define CLAP_BURST_ON       32U     /* 2 ms on  */
#define CLAP_BURST_OFF      48U     /* 3 ms gap */
#define CLAP_BURST_PERIOD   (CLAP_BURST_ON + CLAP_BURST_OFF)   /* 80 samp */
#define CLAP_NUM_BURSTS     3U
#define CLAP_BURST_TOTAL    (CLAP_NUM_BURSTS * CLAP_BURST_PERIOD)  /* 240 */
#define CLAP_TAIL_DECAY     DECAY_TAU_2048

/* Envelope floor — below this the voice is silenced (~−54 dB) */
#define ENV_FLOOR  64

/* ===== Per-voice runtime state =========================================== */
typedef struct {
  uint8_t  active;
  uint32_t n;           /* sample counter since trigger                      */
  int32_t  env;         /* primary envelope   0 … 32767                      */
  int32_t  env2;        /* secondary envelope (snare noise component)        */
  uint32_t phase_acc;   /* Q16.16 phase accumulator                          */
  uint32_t phase_inc;   /* Q16.16 current phase increment (may sweep)        */
  int32_t  hpf_z1;      /* LPF state for hi-hat HPF-via-subtraction          */
} DrumVoice_t;

static DrumVoice_t voices[DRUM_COUNT];

/* ===== Individual voice sample generators ================================ */

static int32_t Kick_Sample(DrumVoice_t *v)
{
  if (!v->active) return 0;

  /* ---- pitch sweep: exponential decay toward KICK_FREQ_END ---- */
  if (v->phase_inc > KICK_FREQ_END) {
    uint32_t delta = v->phase_inc - KICK_FREQ_END;
    delta -= delta >> KICK_PITCH_SHIFT;
    v->phase_inc = KICK_FREQ_END + delta;
  }

  /* ---- sine oscillator ---- */
  uint8_t idx = (uint8_t)(v->phase_acc >> 16);
  v->phase_acc += v->phase_inc;
  int32_t sample = (int32_t)sine_tab[idx];

  /* ---- amplitude envelope ---- */
  sample = (sample * v->env) >> 15;
  v->env = (int32_t)((uint32_t)v->env * KICK_AMP_DECAY >> 16);

  if (v->env < ENV_FLOOR) { v->active = 0; return 0; }
  return sample;
}

static int32_t Snare_Sample(DrumVoice_t *v)
{
  if (!v->active) return 0;

  /* ---- tonal body (sine at 185 Hz, fast decay) ---- */
  uint8_t idx = (uint8_t)(v->phase_acc >> 16);
  v->phase_acc += v->phase_inc;
  int32_t tone = ((int32_t)sine_tab[idx] * v->env) >> 15;
  v->env = (int32_t)((uint32_t)v->env * SNARE_TONE_DECAY >> 16);

  /* ---- noise snap (slower decay) ---- */
  int32_t noise = ((int32_t)Noise_Next() * v->env2) >> 15;
  v->env2 = (int32_t)((uint32_t)v->env2 * SNARE_NOISE_DECAY >> 16);

  if (v->env < ENV_FLOOR && v->env2 < ENV_FLOOR) { v->active = 0; return 0; }

  /* 25 % tone + 75 % noise — gives body without masking the snap */
  return (tone + 3 * noise) >> 2;
}

static int32_t HiHat_Sample(DrumVoice_t *v)
{
  if (!v->active) return 0;

  int32_t raw = (int32_t)Noise_Next();

  /* ---- 1-pole HPF via LPF subtraction (fc ≈ 3 kHz) ---- */
  int32_t diff = raw - v->hpf_z1;
  v->hpf_z1 += ((int32_t)HIHAT_HPF_ALPHA * diff) >> 15;
  int32_t hpf = raw - v->hpf_z1;

  /* ---- amplitude envelope ---- */
  hpf = (hpf * v->env) >> 15;
  v->env = (int32_t)((uint32_t)v->env * HIHAT_AMP_DECAY >> 16);

  if (v->env < ENV_FLOOR) { v->active = 0; return 0; }
  return hpf;
}

static int32_t Clap_Sample(DrumVoice_t *v)
{
  if (!v->active) return 0;

  uint32_t n = v->n++;
  int32_t env;

  if (n < CLAP_BURST_TOTAL) {
    /* ---- burst phase: short noise bursts separated by silence ---- */
    uint32_t pos = n % CLAP_BURST_PERIOD;
    if (pos < CLAP_BURST_ON) {
      env = 32767;
    } else {
      return 0;            /* silence between bursts */
    }
  } else {
    /* ---- tail phase: exponentially decaying noise ---- */
    env = v->env;
    v->env = (int32_t)((uint32_t)env * CLAP_TAIL_DECAY >> 16);
    if (env < ENV_FLOOR) { v->active = 0; return 0; }
  }

  return ((int32_t)Noise_Next() * env) >> 15;
}

/* ===== Public API ======================================================== */

void DrumSynth_Init(void)
{
  memset(voices, 0, sizeof(voices));
  lfsr = 0xACE1U;
}

void DrumSynth_Trigger(DrumType_t drum)
{
  if (drum >= DRUM_COUNT) return;
  DrumVoice_t *v = &voices[drum];

  v->active    = 1;
  v->n         = 0;
  v->env       = 32767;
  v->env2      = 32767;
  v->phase_acc = 0;
  v->hpf_z1    = 0;

  switch (drum) {
    case DRUM_KICK:   v->phase_inc = KICK_FREQ_START;  break;
    case DRUM_SNARE:  v->phase_inc = SNARE_TONE_FREQ;  break;
    default:          v->phase_inc = 0;                 break;
  }
}

int16_t DrumSynth_GetNextSample(void)
{
  int32_t mix = 0;
  mix += Kick_Sample(&voices[DRUM_KICK]);
  mix += Snare_Sample(&voices[DRUM_SNARE]);
  mix += HiHat_Sample(&voices[DRUM_HIHAT]);
  mix += Clap_Sample(&voices[DRUM_CLAP]);

  mix >>= 1;   /* 6 dB headroom for multi-voice overlap */

  if (mix >  32767) mix =  32767;
  if (mix < -32768) mix = -32768;

  return (int16_t)mix;
}
