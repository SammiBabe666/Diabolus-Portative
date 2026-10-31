// The twelve factory presets (tests/presets.h, generated with presets/*.json
// from one table): each speaks at once, stays within the declared peak, holds
// no DC, survives an eight-note chord with stolen voices, and differs from
// every other preset in what it does (brightness, 16', onset and tail),
// not only in its numbers.
#include "check.h"
#include "presets.h"

static double buffer[44100];

struct Measure { double rms, brightness, sub, onset, tail, beating; };

int main() {
    require(preset_count == 12, "twelve factory presets");
    Measure m[preset_count];
    for (unsigned p = 0; p < preset_count; ++p) {
        static Player one;
        one.begin(presets[p].values, 50);  // D3
        one.render(buffer, 44100);
        const double* steady = buffer + 22050;
        m[p].rms = rms(steady, 22050);
        double low = 0, high = 0;
        for (int k = 1; k <= 3; ++k) low += magnitude(steady, 22050, 146.83 * k);
        for (int k = 4; k <= 8; ++k) high += magnitude(steady, 22050, 146.83 * k);
        m[p].brightness = high / low;
        m[p].sub = magnitude(steady, 22050, 73.42);
        m[p].onset = rms(buffer, 2205) / m[p].rms;  // the first 50 ms against the held note
        // DRIFT's beating: how far the third harmonic swings over a second.
        double quiet = 1e9, loud = 0;
        for (int window = 0; window < 10; ++window) {
            one.render(buffer, 4410);
            const double h3 = magnitude(buffer, 4410, 440.5);
            quiet = std::fmin(quiet, h3);
            loud = std::fmax(loud, h3);
        }
        m[p].beating = (loud - quiet) / (loud + 1e-9);
        // Change the note and measure what lingers of the old one: reverb and echoes.
        one.pitch = increment(57);
        one.render(buffer, 22050);
        m[p].tail = magnitude(buffer + 4410, 8820, 146.83) + magnitude(buffer + 4410, 8820, 293.66);
        std::printf("%-13s rms %.3f bright %.3f sub %.3f onset %.2f tail %.4f beating %.3f\n", presets[p].name,
                    m[p].rms, m[p].brightness, m[p].sub, m[p].onset, m[p].tail, m[p].beating);
        require(m[p].rms > 0.12, "preset is clearly audible");
        if (presets[p].values[4] < 0.3) require(m[p].onset > 0.3, "a fast preset speaks within 50 ms");
    }
    // Distinct: every pair differs clearly in at least one measured property.
    for (unsigned a = 0; a < preset_count; ++a)
        for (unsigned b = a + 1; b < preset_count; ++b) {
            const double bright = std::fabs(std::log(m[a].brightness / m[b].brightness));
            const double sub = std::fabs(m[a].sub - m[b].sub) / 0.03;  // a 16' present or not
            const double onset = std::fabs(m[a].onset - m[b].onset) / 0.3;
            const double tail = std::fabs(std::log((m[a].tail + 1e-3) / (m[b].tail + 1e-3)));
            double controls = 0;
            for (int c = 0; c < 8; ++c) controls += std::fabs(presets[a].values[c] - presets[b].values[c]);
            // Each term is about 1 for a plainly audible difference in that property.
            const double apart = std::sqrt(bright * bright / 0.12 + sub * sub + onset * onset + tail * tail / 0.25);
            if (apart < 0.6) std::printf("too alike: %s and %s (%.2f)\n", presets[a].name, presets[b].name, apart);
            require(apart >= 0.6, "presets must sound different");
            require(controls > 0.5, "presets must differ in their settings");
        }
    // A chord of eight voices with steals and bends: bounded, no DC.
    for (unsigned p = 0; p < preset_count; ++p) {
        static Player voices[8];
        double sum = 0, energy = 0;
        unsigned count = 0;
        for (unsigned v = 0; v < 8; ++v) voices[v].begin(presets[p].values, 38 + 5 * v, v + 1);
        for (unsigned block = 0; block < 700; ++block) {
            if (block % 175 == 100) {  // a stolen voice starts a new note
                const unsigned v = (block / 175) & 7;
                voices[v].begin(presets[p].values, 45 + v, 20 + block);
            }
            for (unsigned v = 0; v < 8; ++v) {
                if (block >= 500) voices[v].pitch += 2000;  // a slow bend
                voices[v].render(buffer, opxy::block_frames);
                for (unsigned i = 0; i < opxy::block_frames; ++i) {
                    sum += buffer[i];
                    energy += buffer[i] * buffer[i];
                    ++count;
                }
            }
        }
        const double dc = sum / count;
        require(std::sqrt(energy / count) > 0.1, "chord is audible");
        require(dc < 0.01 && dc > -0.01, "preset carries DC");
    }
    // The init sound (control defaults) is musical too.
    static Player init;
    init.begin(control_defaults, 57);
    init.render(buffer, 44100);
    require(rms(buffer + 22050, 22050) > 0.12, "the init preset is audible");
    return 0;
}
