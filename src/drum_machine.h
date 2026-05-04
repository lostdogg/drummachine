#pragma once

#include <stdint.h>
#include <stdbool.h>

// ---------------------------------------------------------------------------
// Drum-machine sequencer
// ---------------------------------------------------------------------------
// 16-step, 4-track (kick / snare / hi-hat / clap) pattern sequencer.
// Audio is generated as simple synthesised waveforms (no sample flash needed).
// ---------------------------------------------------------------------------

// Track indices
#define TRACK_KICK   0
#define TRACK_SNARE  1
#define TRACK_HIHAT  2
#define TRACK_CLAP   3

// Sequencer state
typedef struct {
    bool  pattern[4][16];  // pattern[track][step] -> active?
    int   current_step;    // 0..15
    int   bpm;             // current tempo
    bool  running;         // playback active?
    float master_volume;   // 0.0 .. 1.0
    int   pattern_bank;    // 0..3 (selected via pot)
    float pitch_factor;    // 0.5 .. 2.0 (transpose all hits)
} DrumSequencer;

// Initialise sequencer with default BPM and a classic 4/4 pattern.
void seq_init(DrumSequencer *seq, int bpm);

// Update BPM from a normalised pot value (0..4095 -> BPM_MIN..BPM_MAX).
void seq_set_bpm_from_pot(DrumSequencer *seq, uint16_t pot_value);

// Update master volume from a normalised pot value (0..4095 -> 0.0..1.0).
void seq_set_volume_from_pot(DrumSequencer *seq, uint16_t pot_value);

// Select pattern bank from a normalised pot value (0..4095 -> 0..3).
void seq_set_pattern_from_pot(DrumSequencer *seq, uint16_t pot_value);

// Update pitch factor from a normalised pot value (0..4095 -> 0.5..2.0).
void seq_set_pitch_from_pot(DrumSequencer *seq, uint16_t pot_value);

// Toggle play/stop.
void seq_toggle_running(DrumSequencer *seq);

// Advance one step and return the bitmask of active tracks (bits 0-3).
// Returns 0 if the sequencer is not running.
uint8_t seq_advance(DrumSequencer *seq);

// Fill an audio buffer (stereo 16-bit interleaved) with the combined hit
// waveforms for the given track bitmask.
// buf_samples: number of stereo sample PAIRS to fill.
void seq_render_audio(DrumSequencer *seq, uint8_t active_tracks,
                      int16_t *buf, uint32_t buf_samples);

// Microseconds between steps at the current BPM (one step = one 16th note).
uint32_t seq_step_interval_us(const DrumSequencer *seq);
