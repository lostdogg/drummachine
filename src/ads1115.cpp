#include "ads1115.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include <stdint.h>
#include <stdbool.h>

// ---------------------------------------------------------------------------
// ADS1115 driver implementation
// ---------------------------------------------------------------------------

// Map channel index to MUX bits (high byte, bits [6:4])
static const uint8_t channel_mux[4] = {
    (uint8_t)ADS1115_MUX_AIN0,  // AIN0 vs GND: MUX = 100
    (uint8_t)ADS1115_MUX_AIN1,  // AIN1 vs GND: MUX = 101
    (uint8_t)ADS1115_MUX_AIN2,  // AIN2 vs GND: MUX = 110
    (uint8_t)ADS1115_MUX_AIN3,  // AIN3 vs GND: MUX = 111
};

bool ads1115_init(i2c_inst_t *i2c, uint8_t addr) {
    // Perform a zero-byte write to check for ACK from device.
    uint8_t buf[1] = { ADS1115_REG_CONFIG };
    int ret = i2c_write_timeout_us(i2c, addr, buf, 1, false, 5000);
    return (ret == 1);
}

int16_t ads1115_read_channel(i2c_inst_t *i2c, uint8_t addr, uint8_t channel) {
    if (channel > 3) channel = 0;

    // Build 16-bit config word (high byte first on the wire):
    //  Bit 15   : OS = 1 (start single conversion)
    //  Bits 14:12: MUX for requested channel (single-ended vs GND)
    //  Bits 11:9 : PGA = 001 (+/-4.096 V full-scale)
    //  Bit  8   : MODE = 1 (single-shot)
    //  Bits 7:5  : DR = 000 (128 SPS; slowest rate reduces noise)
    //  Bits 1:0  : COMP_QUE = 11 (disable comparator)
    uint8_t high = (uint8_t)(ADS1115_OS_SINGLE |
                             channel_mux[channel] |
                             ADS1115_PGA_4096     |
                             ADS1115_MODE_SINGLE);
    uint8_t low  = (uint8_t)(ADS1115_DR_128SPS | 0x03);  // DR=000, comparator disabled

    uint8_t buf[3] = { ADS1115_REG_CONFIG, high, low };
    i2c_write_timeout_us(i2c, addr, buf, 3, false, 10000);

    // Wait for conversion to complete (~8 ms at 128 SPS).
    // Poll the OS bit in the config register until it reads 1.
    uint8_t reg = ADS1115_REG_CONFIG;
    uint8_t status[2];
    for (int timeout = 0; timeout < 20; ++timeout) {
        sleep_ms(1);
        i2c_write_timeout_us(i2c, addr, &reg, 1, true, 5000);
        i2c_read_timeout_us(i2c, addr, status, 2, false, 5000);
        if (status[0] & 0x80) break;  // OS bit set -> conversion done
    }

    // Read conversion register
    reg = ADS1115_REG_CONVERSION;
    i2c_write_timeout_us(i2c, addr, &reg, 1, true, 5000);
    uint8_t data[2];
    i2c_read_timeout_us(i2c, addr, data, 2, false, 5000);

    return (int16_t)((data[0] << 8) | data[1]);
}

uint16_t ads1115_read_normalised(i2c_inst_t *i2c, uint8_t addr, uint8_t channel) {
    // Full-scale at +4.096 V = 32767; 3.3 V rail -> ~26378 counts.
    // We map 0..26378 to 0..4095 for a clean 12-bit range.
    int16_t raw = ads1115_read_channel(i2c, addr, channel);
    if (raw < 0) raw = 0;
    // Scale: 4095 * raw / 26378  (use 32-bit to avoid overflow)
    uint32_t scaled = ((uint32_t)raw * 4095u) / 26378u;
    if (scaled > 4095u) scaled = 4095u;
    return (uint16_t)scaled;
}
