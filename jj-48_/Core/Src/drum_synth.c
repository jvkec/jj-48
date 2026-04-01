/**
  ******************************************************************************
  * @file    drum_synth.c
  * @brief   4 voices kick snare hat clap nothing fancy
  *
  * kick: sine sweep 200 down to 50ish hz, xfade on retrigger so it doesnt click
  *       amp env ~64ms-ish feels punchy enough
  * snare: 185hz sine (dies fast) + lfsr noise slower tail, body vs snap you know
  * hat: noise thru 1pole hpf fc aprox 3k short env ~32ms closed hat vibe
  * clap: 3 little noise blips 2ms on 3ms off then long noisy tail 128ms tau
  *
  * took ideas from demofox drum post
  * https://blog.demofox.org/2015/03/14/diy-synth-basic-drum/
  ******************************************************************************
  */

#include "drum_synth.h"
#include "stm32f4xx_hal.h"
#include <string.h>

/* triggers get ORd in from tim6/main, actually applied at top of GetNextSample (same
 * context as dma audio path) so were not half updating voice structs mid buffer */
static volatile uint32_t g_pending_triggers;

static inline void pending_or(uint32_t mask)
{
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  g_pending_triggers |= mask;
  __set_PRIMASK(primask);
}

static inline uint32_t pending_take_all(void)
{
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  uint32_t p         = g_pending_triggers;
  g_pending_triggers = 0U;
  __set_PRIMASK(primask);
  return p;
}

/* sine table 256 pts q15 */
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

/* 16bit mls lfsr period 2^16-1 */
static uint16_t lfsr = 0xACE1U;

static inline int16_t Noise_Next(void)
{
  /* poly x^16+x^14+x^13+x^11+1 taps 0,2,3,5 fibonacci style */
  uint16_t bit = (lfsr ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5)) & 1U;
  lfsr = (lfsr >> 1) | (bit << 15);
  return (int16_t)lfsr;   /* treat as signed noise-ish */
}

/* phase acc stuff */
/*
 * q16.16 phase, high byte indexes sine_tab
 * inc = hz * 256 * 65536 / 16000 = hz * 131072 / 125 all ints no float thank god
 */
#define FREQ_TO_INC(hz)  ((uint32_t)((uint32_t)(hz) * 131072U / 125U))

/* exp decay mults q16 */
/*
 * env *= RATE each sample (>>16)
 * tau in samples roughy 65536/(65536-RATE) at 16khz divide by 16 for ms ballpark
 */
#define DECAY_TAU_256   65280U   /* 256 samp ~16ms */
#define DECAY_TAU_512   65408U   /* 512 ~32ms */
#define DECAY_TAU_1024  65472U   /* 1024 ~64ms */
#define DECAY_TAU_2048  65504U   /* 2048 ~128ms */

/* knobs per drum */

/* kick */
#define KICK_FREQ_START     FREQ_TO_INC(200)
#define KICK_FREQ_END       FREQ_TO_INC(50)
#define KICK_PITCH_SHIFT    8U          /* sweep speed */
#define KICK_AMP_DECAY      DECAY_TAU_1024
#define KICK_ATTACK_LEN     48U         /* ~3ms attack curve */

/* snare */
#define SNARE_TONE_FREQ     FREQ_TO_INC(185)
#define SNARE_TONE_DECAY    DECAY_TAU_256
#define SNARE_NOISE_DECAY   DECAY_TAU_1024

/* hat */
#define HIHAT_AMP_DECAY     DECAY_TAU_512
#define HIHAT_HPF_ALPHA     17726U  /* lpf coeff for hpf trick ~3k fc @16k */

/* clap */
#define CLAP_BURST_ON       32U     /* 2ms */
#define CLAP_BURST_OFF      48U     /* 3ms gap */
#define CLAP_BURST_PERIOD   (CLAP_BURST_ON + CLAP_BURST_OFF)   /* 80 samp */
#define CLAP_NUM_BURSTS     3U
#define CLAP_BURST_TOTAL    (CLAP_NUM_BURSTS * CLAP_BURST_PERIOD)  /* 240 */
#define CLAP_TAIL_DECAY     DECAY_TAU_2048

/* kill voice when env this low ~ -54db or whatever */
#define ENV_FLOOR  64

/* kick retrig: fade old sample out over this many samp (~2ms) stops clicks */
#define KICK_XFADE_LEN  32U

typedef struct {
  uint8_t  active;
  uint32_t n;           /* samples since hit */
  int32_t  env;         /* main env */
  int32_t  env2;        /* snare noise env */
  uint32_t phase_acc;   /* osc phase q16.16 */
  uint32_t phase_inc;   /* step, kick sweeps this */
  int32_t  hpf_z1;      /* hat lpf state */
  int32_t  last_out;    /* kick prev out for xfade */
  int32_t  xfade_val;   /* grabbed at retrig */
  uint32_t xfade_rem;   /* xfade countdown */
} DrumVoice_t;

static DrumVoice_t voices[DRUM_COUNT];

static int32_t Kick_Sample(DrumVoice_t *v)
{
  if (!v->active) return 0;

  uint32_t kn = v->n;
  if (kn < KICK_ATTACK_LEN) {
    v->n = kn + 1U;
  }

  /* pitch pulls down toward end freq */
  if (v->phase_inc > KICK_FREQ_END) {
    uint32_t delta = v->phase_inc - KICK_FREQ_END;
    delta -= delta >> KICK_PITCH_SHIFT;
    v->phase_inc = KICK_FREQ_END + delta;
  }

  /* sine */
  uint8_t idx = (uint8_t)(v->phase_acc >> 16);
  v->phase_acc += v->phase_inc;
  int32_t sample = (int32_t)sine_tab[idx];

  /* amp env */
  sample = (sample * v->env) >> 15;
  if (kn < KICK_ATTACK_LEN) {
    int32_t a = (int32_t)(kn + 1U);
    int32_t d = (int32_t)KICK_ATTACK_LEN * (int32_t)KICK_ATTACK_LEN;
    sample = (sample * a * a) / d;
  }
  v->env = (int32_t)((uint32_t)v->env * KICK_AMP_DECAY >> 16);

  /* xfade old tail so retrig isnt a step */
  if (v->xfade_rem > 0U) {
    sample += (v->xfade_val * (int32_t)v->xfade_rem) / (int32_t)KICK_XFADE_LEN;
    v->xfade_rem--;
  }

  v->last_out = sample;

  if (v->env < ENV_FLOOR) {
    v->active   = 0;
    v->env      = 0;
    v->last_out = 0;
  }
  return sample;
}

static int32_t Snare_Sample(DrumVoice_t *v)
{
  if (!v->active) return 0;

  /* tone */
  uint8_t idx = (uint8_t)(v->phase_acc >> 16);
  v->phase_acc += v->phase_inc;
  int32_t tone = ((int32_t)sine_tab[idx] * v->env) >> 15;
  v->env = (int32_t)((uint32_t)v->env * SNARE_TONE_DECAY >> 16);

  /* noise */
  int32_t noise = ((int32_t)Noise_Next() * v->env2) >> 15;
  v->env2 = (int32_t)((uint32_t)v->env2 * SNARE_NOISE_DECAY >> 16);

 /* 1 part tone 3 parts noise (>>2) */
  int32_t out = (tone + 3 * noise) >> 2;
  if (v->env < ENV_FLOOR && v->env2 < ENV_FLOOR) {
    v->active = 0;
    v->env    = 0;
    v->env2   = 0;
  }
  return out;
}

static int32_t HiHat_Sample(DrumVoice_t *v)
{
  if (!v->active) return 0;

  int32_t raw = (int32_t)Noise_Next();

  /* hpf = raw - lpf(raw) kinda thing */
  int32_t diff = raw - v->hpf_z1;
  v->hpf_z1 += ((int32_t)HIHAT_HPF_ALPHA * diff) >> 15;
  int32_t hpf = raw - v->hpf_z1;

  /* env */
  hpf = (hpf * v->env) >> 15;
  v->env = (int32_t)((uint32_t)v->env * HIHAT_AMP_DECAY >> 16);

  if (v->env < ENV_FLOOR) {
    v->active = 0;
    v->env    = 0;
  }
  return hpf;
}

static int32_t Clap_Sample(DrumVoice_t *v)
{
  if (!v->active) return 0;

  uint32_t n = v->n++;
  int32_t env;

  if (n < CLAP_BURST_TOTAL) {
    /* blip blip blip */
    uint32_t pos = n % CLAP_BURST_PERIOD;
    if (pos < CLAP_BURST_ON) {
      env = 32767;
    } else {
      return 0;            /* gap */
    }
  } else {
    /* tail decays */
    env = v->env;
    v->env = (int32_t)((uint32_t)env * CLAP_TAIL_DECAY >> 16);
  }

  int32_t out = ((int32_t)Noise_Next() * env) >> 15;
  if (n >= CLAP_BURST_TOTAL && v->env < ENV_FLOOR) {
    v->active = 0;
    v->env    = 0;
  }
  return out;
}

void DrumSynth_Init(void)
{
  memset(voices, 0, sizeof(voices));
  lfsr = 0xACE1U;
  (void)pending_take_all();
}

static void DrumSynth_DoTrigger(DrumType_t drum)
{
  DrumVoice_t *v = &voices[drum];

  if (drum == DRUM_KICK && v->active) {
    v->xfade_val = v->last_out;
    v->xfade_rem = KICK_XFADE_LEN;
  }

  v->active = 1;
  v->n      = 0;
  v->env    = 32767;
  v->env2   = 32767;
  v->hpf_z1 = 0;

  switch (drum) {
    case DRUM_KICK:
      v->phase_acc = 0;
      v->phase_inc = KICK_FREQ_START;
      break;
    case DRUM_SNARE:
      v->phase_acc = 0;
      v->phase_inc = SNARE_TONE_FREQ;
      break;
    default:
      v->phase_acc = 0;
      v->phase_inc = 0;
      break;
  }
}

static void DrumSynth_FlushPending(void)
{
  uint32_t p = pending_take_all();
  for (uint32_t t = 0U; t < (uint32_t)DRUM_COUNT; t++) {
    if ((p & (1U << t)) != 0U) {
      DrumSynth_DoTrigger((DrumType_t)t);
    }
  }
}

void DrumSynth_Trigger(DrumType_t drum)
{
  if (drum >= DRUM_COUNT) {
    return;
  }
  pending_or(1U << (unsigned)drum);
}

int16_t DrumSynth_GetNextSample(void)
{
  DrumSynth_FlushPending();

  int32_t mix = 0;
  mix += Kick_Sample(&voices[DRUM_KICK]);
  mix += Snare_Sample(&voices[DRUM_SNARE]);
  mix += HiHat_Sample(&voices[DRUM_HIHAT]);
  mix += Clap_Sample(&voices[DRUM_CLAP]);

  mix >>= 2;

  if (mix >  32767) mix =  32767;
  if (mix < -32768) mix = -32768;

  return (int16_t)mix;
}
