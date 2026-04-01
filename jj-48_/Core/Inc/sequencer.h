/**
 * @file sequencer.h
 * @brief 8-step pattern grid and TIM6 step callback (drum triggers).
 */
#ifndef SEQUENCER_H
#define SEQUENCER_H

#include "drum_synth.h"
#include <stdint.h>

#define SEQUENCER_NUM_STEPS 8U

/* Keypad/UI will toggle these; TIM6 ISR reads them. */
extern volatile uint8_t pattern[DRUM_COUNT][SEQUENCER_NUM_STEPS];
extern volatile uint8_t current_step;

/* Zero the step grid and reset the playhead (no steps armed). */
void Sequencer_Init(void);

#endif /* SEQUENCER_H */
