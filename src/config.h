#pragma once

// ---------------------------------------------------------------------------
// Hardware pin / address configuration
// Matches the wiring spec in the project documentation.
// ---------------------------------------------------------------------------

// ---- I2C bus (i2c0) -------------------------------------------------------
// Pico master -> CYD GPIO 21/22 & ADS1115 SDA/SCL
#define I2C_BUS       i2c0
#define I2C_SDA_PIN   4       // GPIO 4  (Physical Pin 6)
#define I2C_SCL_PIN   5       // GPIO 5  (Physical Pin 7)
#define I2C_FREQ_HZ   400000  // 400 kHz fast-mode

// ---- ADS1115 ADC (ADDR tied to GND -> address 0x48) ----------------------
#define ADS1115_ADDR  0x48

// Potentiometer channels wired to ADS1115 inputs
#define POT_TEMPO     0  // A0 – BPM control
#define POT_VOLUME    1  // A1 – master volume
#define POT_PATTERN   2  // A2 – pattern / bank select
#define POT_PITCH     3  // A3 – global pitch / filter

// ---- I2S audio output (PIO) -> PCM5102A ----------------------------------
// PCM5102A SCK pin must be tied to GND (internal clock mode).
#define I2S_BCK_PIN   26  // GPIO 26 (Physical Pin 31) -> PCM5102A BCK
#define I2S_LRCK_PIN  27  // GPIO 27 (Physical Pin 32) -> PCM5102A LRCK
#define I2S_DIN_PIN   28  // GPIO 28 (Physical Pin 34) -> PCM5102A DIN

#define AUDIO_SAMPLE_RATE  44100
#define AUDIO_BIT_DEPTH    16

// ---- Button --------------------------------------------------------------
// Wired directly between GPIO 15 and GND; internal pull-up enabled in code.
#define BUTTON_PIN   15  // GPIO 15 (Physical Pin 20)

// ---- Sequencer -----------------------------------------------------------
#define SEQ_STEPS       16   // steps per bar
#define SEQ_TRACKS       4   // kick, snare, hi-hat, clap
#define SEQ_BPM_MIN     60
#define SEQ_BPM_MAX    200
#define SEQ_BPM_DEFAULT 120
