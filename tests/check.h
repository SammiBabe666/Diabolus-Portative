// Shared helpers for Diabolus Portative's host tests. Tests use require(), never assert.
#pragma once
#include "../engine.cpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using diabolus::i32;
using diabolus::u32;

static inline void require(bool ok, const char* message) {
    if (!ok) {
        std::fprintf(stderr, "FAIL %s\n", message);
        std::exit(1);
    }
}

static const double peak = 1073741824.0;  // definition.json outputPeakQ31: half scale
static const double rate = 44100.0;

// Q31 control word for a normalised value.
static inline i32 q31(double value) { return static_cast<i32>(std::lrint(value * 2147483647.0)); }

// Phase increment for a MIDI note at 44.1 kHz.
static inline u32 increment(double midi) {
    return static_cast<u32>(std::llrint(440.0 * std::pow(2.0, (midi - 69) / 12) / rate * 4294967296.0));
}

// One voice, freshly zeroed as the SDK hands it out, with its controls and pitch.
struct Player {
    diabolus::Voice voice;
    diabolus::Shared shared;
    i32 controls[8];
    u32 pitch;
    void begin(const double (&values)[8], double midi, u32 owner = 1) {
        std::memset(&voice, 0, sizeof voice);
        set(values);
        pitch = increment(midi);
        diabolus::initialize(voice, shared, owner);
        diabolus::start(voice, shared, owner);
    }
    void set(const double (&values)[8]) {
        for (int c = 0; c < 8; ++c) controls[c] = q31(values[c]);
    }
    // Renders whole blocks into out (as fractions of the declared peak), checking bounds.
    void render(double* out, unsigned frames) {
        u32 pitches[opxy::block_frames];
        i32 block[opxy::block_frames];
        for (unsigned i = 0; i < opxy::block_frames; ++i) pitches[i] = pitch;
        for (unsigned at = 0; at < frames; at += opxy::block_frames) {
            diabolus::render(voice, shared, 1, block, controls, pitches);
            for (unsigned i = 0; i < opxy::block_frames && at + i < frames; ++i) {
                require(block[i] >= -1073741824 && block[i] <= 1073741824, "output exceeds the declared peak");
                if (out) out[at + i] = block[i] / peak;
            }
        }
    }
};

// Goertzel magnitude of one frequency (as a fraction of the declared peak),
// through a Hann window so neighbouring partials do not leak in.
static inline double magnitude(const double* x, unsigned n, double hz) {
    const double pi = 3.14159265358979323846, w = 2 * pi * hz / rate, c = 2 * std::cos(w);
    double s1 = 0, s2 = 0;
    for (unsigned i = 0; i < n; ++i) {
        const double s0 = x[i] * (1 - std::cos(2 * pi * i / n)) + c * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    return 2 * std::sqrt(std::fmax(0.0, s1 * s1 + s2 * s2 - c * s1 * s2)) / n;
}

static inline double rms(const double* x, unsigned n) {
    double e = 0;
    for (unsigned i = 0; i < n; ++i) e += x[i] * x[i];
    return std::sqrt(e / n);
}
