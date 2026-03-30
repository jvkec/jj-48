# jj-48

An 8-step drum sequencer on an STM32F446 NUCLEO-144.

- **Keypad** moves a cursor on a 4×8 **OLED** grid and toggles steps.
- **Potentiometer** sets BPM. 
- **TIM6** advances the playhead and triggers hits from **`pattern[][]`**.
- **I2S + DMA** streams mixed samples from a fixed-point **drum synthesizer** to a **MAX98357** amplifier. 
- Main loop scans the keypad, polls the ADC for tempo, and refreshes the display. 
- Audio is paced independently by DMA so UI work does not block output.

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
