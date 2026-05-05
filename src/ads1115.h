#pragma once

#include "hardware/i2c.h"
#include <stdint.h>

// ---------------------------------------------------------------------------
// Minimal ADS1115 driver
// Communicates over I2C with a device at the address set by config.h.
// Only single-ended, single-shot mode is used (sufficient for 4 pots).
// ---------------------------------------------------------------------------

// ADS1115 register addresses
#define ADS1115_REG_CONVERSION  0x00
#define ADS1115_REG_CONFIG      0x01

// Config register bit fields (written as 16-bit big-endian)
// MUX[14:12]: single-ended channel selection (AINx vs GND)
#define ADS1115_MUX_AIN0  (0x4 << 4)  // 100 -> AIN0/GND
#define ADS1115_MUX_AIN1  (0x5 << 4)  // 101 -> AIN1/GND
#define ADS1115_MUX_AIN2  (0x6 << 4)  // 110 -> AIN2/GND
#define ADS1115_MUX_AIN3  (0x7 << 4)  // 111 -> AIN3/GND

// PGA[11:9]: +/-4.096 V full-scale (matches 3.3 V pot supply)
#define ADS1115_PGA_4096  (0x1 << 1)

// MODE[8]: single-shot
#define ADS1115_MODE_SINGLE  (0x1)

// DR[7:5]: 128 SPS (slowest – reduces noise for pot reads)
#define ADS1115_DR_128SPS  (0x0 << 5)

// OS[15]: start single conversion
#define ADS1115_OS_SINGLE  (0x1 << 7)  // placed in high byte

// Initialise the I2C peripheral and ADS1115.
// Returns true on success (ACK received from device).
bool ads1115_init(i2c_inst_t *i2c, uint8_t addr);

// Read a single-ended channel (0-3).
// Triggers a single-shot conversion and waits for result.
// Returns raw 16-bit signed value (-32768 .. 32767).
int16_t ads1115_read_channel(i2c_inst_t *i2c, uint8_t addr, uint8_t channel);

// Convenience: normalise a raw ADS1115 reading to 0..4095 (12-bit range).
// Values above 3.3 V (or below 0 V) are clamped.
uint16_t ads1115_read_normalised(i2c_inst_t *i2c, uint8_t addr, uint8_t channel);
