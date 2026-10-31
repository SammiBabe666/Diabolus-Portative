#pragma once
#include <opxy/audio.hpp>

namespace diabolus {
using opxy::i16;
using opxy::i32;
using opxy::u16;
using opxy::u32;

// Nine sine partials: a 16' sub (index 0) and harmonics 1..8 (indices 1..8).
const u32 partial_count = 9;
const u32 control_count = 8;

// The effects run at an eighth of 44.1 kHz (5512 Hz, 8 steps per block):
// a dark room and dark repeats need no more. Line lengths are in those steps.
const u32 delay_length = 1764;  // 320 ms
const u32 diffuser_lengths[2] = {57, 121};
const u32 line_lengths[4] = {300, 354, 411, 488};  // 54 to 89 ms

// Only sound state. The SDK owns allocation and zeroes a voice before each owner.
struct Voice {
    u32 phase;                    // at the 16' pitch: every partial follows it exactly
    i32 weight[partial_count];    // current partial levels, ramped every eight samples
    u32 beat_phase[partial_count];  // slow per-partial beating
    u32 beat_rate[partial_count];
    u32 wind_phase[2];            // slow wind-pressure wander
    i32 control[control_count];   // block-smoothed controls, Q31
    u32 envelope;                 // attack ramp, Q31
    i32 chiff, chiff_rise;        // onset breath: a slow decay less a fast one
    i32 noise_low, noise_previous;
    u32 seed;
    u32 started;                  // a note started: strike the chiff on the next block
    u32 ready;                    // controls and weights are established
    // Effects
    i32 prefilter;                // anti-alias lowpass before the eighth-rate effects
    i32 wet_previous;               // the last effect step of the previous block
    i32 dry_gain, echo_gain, reverb_gain;  // current mix gains, Q15
    i32 echo_low;                 // damping inside the delay loop
    i32 echo_level;               // peak follower that limits regeneration
    i32 line_low[4];              // damping inside each reverb line
    u32 written;                  // effect steps since the note began, up to the longest line
    u16 delay_at, diffuser_at[2], line_at[4];
    u16 padding;
    i16 delay[delay_length];
    i16 diffuser0[57];
    i16 diffuser1[121];
    i16 line0[300];
    i16 line1[354];
    i16 line2[411];
    i16 line3[488];
};
// Voices share nothing: every voice carries its own reverb and delay.
struct Shared {};
}
