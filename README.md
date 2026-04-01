# jj-48

An 8-step drum sequencer on an STM32F446 NUCLEO-144.

- **Keypad** moves a cursor on a 4×8 **OLED** grid and toggles steps.
- **Internal flash memory** stores the current sequence so it persists across resets and power cycles.
- **Potentiometer** sets BPM. 
- **TIM6** advances the playhead and triggers hits from **`pattern[][]`**.
- **I2S + DMA** streams mixed samples from a fixed-point **drum synthesizer** to a **MAX98357** amplifier. 
- Main loop scans the keypad, polls the ADC for tempo, and refreshes the display. 
- Audio is paced independently by DMA so UI work does not block output.

---

## Controls

- `2`, `4`, `6`, `8`: move the cursor around the 4×8 grid.
- `5`: toggle the selected step on or off.
- `*`: clear the entire pattern.
- `USER` button on the NUCLEO board: save the current pattern to internal flash.

On startup, the sequencer reads the saved pattern from STM32 internal flash and restores it automatically if valid data is present. If no saved pattern is found, it starts with an empty pattern.

## Flash Storage

- Pattern data is stored in STM32 internal flash sector 7.
- Flash address range used: **`0x08060000`** to **`0x0807FFFF`**.
- The saved payload contains the full **4 drum lanes × 8 steps** pattern plus a validity marker.

---

## Pinouts

### MAX98357 (I2S3)

| Signal | MCU pin |
|--------|---------|
| LRC (WS) | PA15 |
| BCLK (CK) | PC10 |
| DIN (SD) | PC12 |
| GND | GND |
| VIN | 3V3 |

### BPM potentiometer (ADC1)

| Signal | MCU pin |
|--------|---------|
| Wiper (ADC) | PA3 |
| VCC / one leg | 3V3 |
| GND / other leg | GND |

### Keypad (4×3 matrix)

| Row | MCU pin |
|-----|---------|
| ROW1 | PF15 |
| ROW2 | PE13 |
| ROW3 | PF14 |
| ROW4 | PE11 |

| Column | MCU pin |
|--------|---------|
| COL1 | PF12 |
| COL2 | PD15 |
| COL3 | PD14 |

### OLED (SSD1306, I2C2)

| Signal | MCU pin |
|--------|---------|
| GND | GND |
| VCC | 3V3 |
| SCL | PF1 |
| SDA | PF0 |
