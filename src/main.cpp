#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"

#include "config.h"
#include "ads1115.h"
#include "drum_machine.h"

// PIO program header generated from i2s_output.pio
#include "i2s_output.pio.h"

// ---------------------------------------------------------------------------
// Audio ring buffer
// ---------------------------------------------------------------------------
// Holds audio for one 16th-note step at 44.1 kHz.
// At 120 BPM: step = 125 ms -> 5513 stereo samples.
// We round up to a power-of-two friendly size.
#define AUDIO_BUF_STEREO_SAMPLES 6144  // stereo pairs

static int16_t audio_buf[AUDIO_BUF_STEREO_SAMPLES * 2];  // L + R interleaved

// ---------------------------------------------------------------------------
// Button debounce
// ---------------------------------------------------------------------------
#define DEBOUNCE_MS 30

static bool     button_pressed     = false;
static uint32_t button_last_time   = 0;

static void gpio_callback(uint gpio, uint32_t events) {
    if (gpio != BUTTON_PIN) return;
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if ((now - button_last_time) < DEBOUNCE_MS) return;
    button_last_time = now;
    button_pressed = true;
}

// ---------------------------------------------------------------------------
// I2C initialisation
// ---------------------------------------------------------------------------
static void i2c_setup(void) {
    i2c_init(I2C_BUS, I2C_FREQ_HZ);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    // Enable internal pull-ups (supplement any external 4.7 kΩ resistors)
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);
}

// ---------------------------------------------------------------------------
// I2S / PIO initialisation
// ---------------------------------------------------------------------------
static PIO  i2s_pio;
static uint i2s_sm;

static void i2s_setup(void) {
    i2s_pio = pio0;
    uint offset = pio_add_program(i2s_pio, &i2s_output_program);
    i2s_sm = pio_claim_unused_sm(i2s_pio, true);
    i2s_output_program_init(i2s_pio, i2s_sm, offset,
                            I2S_DIN_PIN, I2S_BCK_PIN,
                            AUDIO_SAMPLE_RATE);
}

// Push one stereo sample pair (32-bit word: left 16 MSB | right 16 LSB)
// into the PIO TX FIFO.  Blocks if the FIFO is full.
static inline void i2s_push_sample(int16_t left, int16_t right) {
    uint32_t word = ((uint32_t)(uint16_t)left << 16) | (uint16_t)right;
    pio_sm_put_blocking(i2s_pio, i2s_sm, word);
}

// ---------------------------------------------------------------------------
// Button initialisation
// ---------------------------------------------------------------------------
static void button_setup(void) {
    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);  // active-low button, no external resistor needed
    gpio_set_irq_enabled_with_callback(BUTTON_PIN,
        GPIO_IRQ_EDGE_FALL, true, gpio_callback);
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main(void) {
    stdio_init_all();

    // ---- Hardware init -----------------------------------------------
    i2c_setup();
    i2s_setup();
    button_setup();

    // ---- ADS1115 init ------------------------------------------------
    bool adc_ok = ads1115_init(I2C_BUS, ADS1115_ADDR);
    if (!adc_ok) {
        // ADC not found; continue with default pot values
        printf("ADS1115 not found – using default pot values\n");
    }

    // ---- Sequencer init ----------------------------------------------
    DrumSequencer seq;
    seq_init(&seq, SEQ_BPM_DEFAULT);

    printf("Drum machine ready. Press GPIO %d button to start/stop.\n",
           BUTTON_PIN);

    // ---- Main loop ---------------------------------------------------
    uint64_t next_step_time = time_us_64();

    while (true) {
        // -- Handle button press ---------------------------------------
        if (button_pressed) {
            button_pressed = false;
            seq_toggle_running(&seq);
            printf("Sequencer %s\n", seq.running ? "STARTED" : "STOPPED");
        }

        // -- Read pots (ADS1115) every ~50 ms -------------------------
        static uint64_t last_pot_read = 0;
        uint64_t now_us = time_us_64();
        if (adc_ok && (now_us - last_pot_read) > 50000u) {
            last_pot_read = now_us;

            uint16_t v_tempo   = ads1115_read_normalised(I2C_BUS, ADS1115_ADDR, POT_TEMPO);
            uint16_t v_volume  = ads1115_read_normalised(I2C_BUS, ADS1115_ADDR, POT_VOLUME);
            uint16_t v_pattern = ads1115_read_normalised(I2C_BUS, ADS1115_ADDR, POT_PATTERN);
            uint16_t v_pitch   = ads1115_read_normalised(I2C_BUS, ADS1115_ADDR, POT_PITCH);

            seq_set_bpm_from_pot(&seq, v_tempo);
            seq_set_volume_from_pot(&seq, v_volume);
            seq_set_pattern_from_pot(&seq, v_pattern);
            seq_set_pitch_from_pot(&seq, v_pitch);
        }

        // -- Sequencer tick -------------------------------------------
        now_us = time_us_64();
        if (seq.running && now_us >= next_step_time) {
            uint32_t interval_us = seq_step_interval_us(&seq);
            next_step_time = now_us + interval_us;

            uint8_t active_tracks = seq_advance(&seq);

            if (active_tracks) {
                // Determine how many samples fit in one step interval
                uint32_t step_samples = (uint32_t)(
                    (uint64_t)AUDIO_SAMPLE_RATE * interval_us / 1000000u);
                if (step_samples > AUDIO_BUF_STEREO_SAMPLES)
                    step_samples = AUDIO_BUF_STEREO_SAMPLES;

                seq_render_audio(&seq, active_tracks,
                                 audio_buf, step_samples);

                for (uint32_t i = 0; i < step_samples; ++i) {
                    i2s_push_sample(audio_buf[2 * i],
                                    audio_buf[2 * i + 1]);
                }
            }
        }

        // -- Idle: push silence to keep the I2S stream alive ----------
        if (!seq.running) {
            i2s_push_sample(0, 0);
        }
    }

    return 0;
}
