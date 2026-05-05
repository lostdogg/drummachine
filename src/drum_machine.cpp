#include "drum_machine.h"
#include "config.h"
#include <string.h>
#include <math.h>
#include <stdint.h>

// ---------------------------------------------------------------------------
// Synthesised drum voices
// ---------------------------------------------------------------------------
// All voices produce float samples in [-1, +1] that are then scaled by
// master_volume and converted to int16_t.

// Kick: exponentially-decaying sine wave starting at ~80 Hz, sweeping down.
static float voice_kick(uint32_t sample_idx, float pitch_factor) {
    const float decay    = 0.0003f;
    const float freq_hz  = 80.0f * pitch_factor;
    float t = (float)sample_idx / (float)AUDIO_SAMPLE_RATE;
    float env = expf(-decay * (float)sample_idx);
    return env * sinf(2.0f * (float)M_PI * freq_hz * t);
}

// Snare: white noise + pitched body.
static float voice_snare(uint32_t sample_idx, float pitch_factor,
                         uint32_t *rng_state) {
    const float decay_body  = 0.0008f;
    const float decay_noise = 0.0005f;
    const float freq_hz     = 180.0f * pitch_factor;
    float t = (float)sample_idx / (float)AUDIO_SAMPLE_RATE;
    // LCG noise
    *rng_state = *rng_state * 1664525u + 1013904223u;
    float noise = ((float)(int32_t)*rng_state / (float)0x80000000u);
    float body  = sinf(2.0f * (float)M_PI * freq_hz * t) *
                  expf(-decay_body * (float)sample_idx);
    float hiss  = noise * expf(-decay_noise * (float)sample_idx);
    return 0.5f * body + 0.5f * hiss;
}

// Hi-hat: short burst of high-frequency noise.
static float voice_hihat(uint32_t sample_idx, uint32_t *rng_state) {
    const float decay = 0.003f;
    *rng_state = *rng_state * 1664525u + 1013904223u;
    float noise = ((float)(int32_t)*rng_state / (float)0x80000000u);
    return noise * expf(-decay * (float)sample_idx);
}

// Clap: very short white-noise burst with tiny pitch.
static float voice_clap(uint32_t sample_idx, float pitch_factor,
                        uint32_t *rng_state) {
    const float decay   = 0.002f;
    const float freq_hz = 600.0f * pitch_factor;
    float t = (float)sample_idx / (float)AUDIO_SAMPLE_RATE;
    *rng_state = *rng_state * 1664525u + 1013904223u;
    float noise = ((float)(int32_t)*rng_state / (float)0x80000000u);
    float tone  = sinf(2.0f * (float)M_PI * freq_hz * t);
    return (0.6f * noise + 0.4f * tone) * expf(-decay * (float)sample_idx);
}

// ---------------------------------------------------------------------------
// Default patterns (4 banks x 4 tracks x 16 steps)
// ---------------------------------------------------------------------------
static const bool default_patterns[4][4][16] = {
    // Bank 0: classic 4/4
    {
        { 1,0,0,0, 1,0,0,0, 1,0,0,0, 1,0,0,0 },  // kick
        { 0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0 },  // snare
        { 1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,1,0 },  // hi-hat
        { 0,0,0,0, 0,0,0,0, 0,0,0,1, 0,0,0,0 },  // clap
    },
    // Bank 1: breakbeat
    {
        { 1,0,0,0, 0,0,1,0, 1,0,0,0, 0,0,1,0 },
        { 0,0,0,0, 1,0,0,0, 0,1,0,0, 1,0,0,0 },
        { 1,1,0,1, 1,0,1,1, 1,1,0,1, 1,0,1,0 },
        { 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,1,0,0 },
    },
    // Bank 2: sparse
    {
        { 1,0,0,0, 0,0,0,0, 1,0,0,0, 0,0,0,0 },
        { 0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,0 },
        { 1,0,0,0, 1,0,0,0, 1,0,0,0, 1,0,0,0 },
        { 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0 },
    },
    // Bank 3: dense / techno
    {
        { 1,0,1,0, 1,0,1,0, 1,0,1,0, 1,0,1,0 },
        { 0,0,0,0, 1,0,0,0, 0,0,0,0, 1,0,0,1 },
        { 1,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1 },
        { 0,0,0,1, 0,0,0,1, 0,0,0,1, 0,0,0,1 },
    },
};

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void seq_init(DrumSequencer *seq, int bpm) {
    memset(seq, 0, sizeof(*seq));
    seq->bpm            = bpm;
    seq->running        = false;
    seq->master_volume  = 0.8f;
    seq->pattern_bank   = 0;
    seq->pitch_factor   = 1.0f;
    seq->current_step   = 0;
    seq->rng_state      = 0xDEADBEEFu;
    // Load default bank 0 pattern
    memcpy(seq->pattern, default_patterns[0], sizeof(seq->pattern));
}

void seq_set_bpm_from_pot(DrumSequencer *seq, uint16_t pot_value) {
    // pot_value 0..4095 -> SEQ_BPM_MIN..SEQ_BPM_MAX
    int range = SEQ_BPM_MAX - SEQ_BPM_MIN;
    seq->bpm  = SEQ_BPM_MIN + (int)(((uint32_t)pot_value * (uint32_t)range) / 4095u);
}

void seq_set_volume_from_pot(DrumSequencer *seq, uint16_t pot_value) {
    seq->master_volume = (float)pot_value / 4095.0f;
}

void seq_set_pattern_from_pot(DrumSequencer *seq, uint16_t pot_value) {
    // pot_value 0..4095 divided into 4 equal ranges of 1024:
    //   0..1023 -> 0,  1024..2047 -> 1,  2048..3071 -> 2,  3072..4095 -> 3
    int bank = (int)((uint32_t)pot_value * 4u / 4096u);  // 0..3
    if (bank > 3) bank = 3;
    if (bank != seq->pattern_bank) {
        seq->pattern_bank = bank;
        memcpy(seq->pattern, default_patterns[bank], sizeof(seq->pattern));
    }
}

void seq_set_pitch_from_pot(DrumSequencer *seq, uint16_t pot_value) {
    // 0..4095 -> 0.5..2.0
    seq->pitch_factor = 0.5f + ((float)pot_value / 4095.0f) * 1.5f;
}

void seq_toggle_running(DrumSequencer *seq) {
    seq->running = !seq->running;
    if (seq->running) seq->current_step = 0;
}

uint8_t seq_advance(DrumSequencer *seq) {
    if (!seq->running) return 0;

    uint8_t active = 0;
    for (int t = 0; t < SEQ_TRACKS; ++t) {
        if (seq->pattern[t][seq->current_step]) {
            active |= (uint8_t)(1u << t);
        }
    }
    seq->current_step = (seq->current_step + 1) % SEQ_STEPS;
    return active;
}

uint32_t seq_step_interval_us(const DrumSequencer *seq) {
    // One step = one 16th note = 60_000_000 / (bpm * 4) microseconds
    return (uint32_t)(60000000u / ((uint32_t)seq->bpm * 4u));
}

void seq_render_audio(DrumSequencer *seq, uint8_t active_tracks,
                      int16_t *buf, uint32_t buf_samples) {
    // Accumulators for each voice (indexed by sample within this buffer)
    for (uint32_t i = 0; i < buf_samples; ++i) {
        float mix = 0.0f;

        if (active_tracks & (1u << TRACK_KICK))
            mix += voice_kick(i, seq->pitch_factor);

        if (active_tracks & (1u << TRACK_SNARE))
            mix += voice_snare(i, seq->pitch_factor, &seq->rng_state);

        if (active_tracks & (1u << TRACK_HIHAT))
            mix += voice_hihat(i, &seq->rng_state);

        if (active_tracks & (1u << TRACK_CLAP))
            mix += voice_clap(i, seq->pitch_factor, &seq->rng_state);

        // Soft clip and scale by master volume
        if (mix >  1.0f) mix =  1.0f;
        if (mix < -1.0f) mix = -1.0f;
        mix *= seq->master_volume;

        int16_t sample = (int16_t)(mix * 32767.0f);

        // Stereo interleaved: left then right (identical)
        buf[2 * i]     = sample;
        buf[2 * i + 1] = sample;
    }
}
