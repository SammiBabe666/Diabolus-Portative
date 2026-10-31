// "Vespers for the Adversary": a witch house tour of Diabolus Portative.
// Every organ sound is the machine's own engine.cpp, rendered offline at
// 44.1 kHz in 64-sample blocks exactly as the OP-XY calls it. The stock amp
// envelope's release and the stock tremolo LFO are emulated here; the sub, the
// drums and the noise swells are synthesized by this renderer for the song.
// Writes song.f32 (stereo float) and timeline.jsonl (one line per video frame).
#include "../../engine.cpp"
#include "../../tests/presets.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
#include <algorithm>

static const double SR = 44100, BPM = 70, STEP = 60.0 / BPM / 4;  // a sixteenth
static const int SECTION = 48, INTRO = 32, OUTRO = 48;          // sixteenths
static const int FPS = 25;
static int total_steps() { return INTRO + 12 * SECTION + OUTRO; }

static int q31(double v) { return (int)std::lrint(v * 2147483647.0); }
static unsigned inc(double midi) { return (unsigned)std::llrint(440.0 * std::pow(2.0, (midi - 69) / 12) / SR * 4294967296.0); }

// One OP-XY voice: the machine's render, then the emulated amp envelope.
struct OrganVoice {
    diabolus::Voice v; diabolus::Shared sh;
    int controls[8]; unsigned pitch = 0; bool gate = false, live = false;
    double amp = 0, release_coef = 0.9999; int owner;
    double block[64]; int at = 64;
    void note_on(double midi, const double* values, int rel, int own) {
        if (!live) { std::memset(&v, 0, sizeof v); diabolus::initialize(v, sh, own); }
        owner = own;
        for (int i = 0; i < 8; ++i) controls[i] = q31(values[i]);
        pitch = inc(midi);
        diabolus::start(v, sh, owner);
        // Stock release: 0.15 s at zero to about 5 s at full, to -60 dB.
        const double seconds = 0.15 + 5.0 * std::pow(rel / 32767.0, 2);
        release_coef = std::exp(std::log(0.001) / (seconds * SR));
        // A retrigger rises from where the release had got to, over 5 ms.
        if (!live) amp = 1;
        gate = live = true;
    }
    void note_off() { gate = false; }
    double tick() {
        if (!live) return 0;
        if (at == 64) {
            unsigned p[64]; int out[64];
            for (int i = 0; i < 64; ++i) p[i] = pitch;
            diabolus::render(v, sh, owner, out, controls, p);
            for (int i = 0; i < 64; ++i) block[i] = out[i] / 1073741824.0;
            at = 0;
        }
        if (gate && amp < 1) amp = std::fmin(1.0, amp + 1 / (0.005 * SR));
        if (!gate) { amp *= release_coef; if (amp < 1e-5) { live = false; amp = 0; at = 64; return 0; } }
        return block[at++] * amp;
    }
};

struct Note { int start, length; std::vector<int> keys; };
// Sparse figures in D Phrygian dominant, three bars each.
static const std::vector<std::vector<Note>> figures = {
    {{0, 12, {62}}, {12, 4, {63}}, {16, 16, {62, 69}}, {36, 4, {70}}, {40, 8, {69}}},
    {{0, 8, {74}}, {8, 8, {73}}, {16, 6, {70}}, {24, 20, {62, 65, 69}}},
    {{0, 24, {50, 57}}, {24, 8, {62}}, {32, 4, {63}}, {36, 12, {61}}},
    {{0, 4, {69}}, {6, 4, {70}}, {12, 4, {69}}, {16, 16, {65}}, {32, 4, {63}}, {36, 12, {62}}},
};
static const int figure_of[12] = {0, 2, 1, 3, 0, 1, 2, 3, 1, 0, 2, 3};
static const int octave_of[12] = {0, -12, 0, 0, 0, 0, -12, 0, 0, 0, -12, 0};
static const bool tremulant[12] = {false, false, false, true, false, true, false, false, true, false, true, false};
// Sections where the drums rest, to let a preset breathe.
static const bool quiet[12] = {true, false, true, false, false, true, false, false, true, false, false, false};

static double frand(unsigned& s) { s = s * 1664525u + 1013904223u; return (s >> 8) / 8388608.0 - 1; }

int main() {
    const int steps = total_steps();
    const long frames = (long)std::ceil(steps * STEP * SR) + (long)(4 * SR);
    std::vector<float> L(frames), R(frames), organ_bus(frames), smooth_sub(frames);
    static OrganVoice voices[8], drone[2];
    int owner = 1;
    const double drone_values[8] = {0.3, 0.9, 0.1, 0.3, 0.75, 0.9, 0, 0};
    // Timeline per video frame.
    const long video_frames = frames * FPS / (long)SR;
    std::vector<int> held(video_frames, 0), preset_at(video_frames, -1), page_at(video_frames, 0);

    // Note events, in samples.
    struct Event { long at; int voice; bool on; double midi; int preset; };
    std::vector<Event> events;
    int next_voice = 0;
    for (int s = 0; s < 12; ++s) {
        const int base = INTRO + s * SECTION;
        for (const Note& n : figures[figure_of[s]]) {
            for (int k : n.keys) {
                const int vi = next_voice; next_voice = (next_voice + 1) % 8;
                events.push_back({(long)((base + n.start) * STEP * SR), vi, true, double(k + octave_of[s]), s});
                events.push_back({(long)((base + n.start + n.length) * STEP * SR) - 64, vi, false, 0, s});
            }
        }
        // Held counts for the screen.
        for (const Note& n : figures[figure_of[s]]) {
            const long a = (long)((base + n.start) * STEP * FPS), b = (long)((base + n.start + n.length) * STEP * FPS);
            for (long f = a; f < b && f < video_frames; ++f) held[f] += (int)n.keys.size();
        }
        const long a = (long)(base * STEP * FPS), b = (long)((base + SECTION) * STEP * FPS);
        for (long f = a; f < b && f < video_frames; ++f) { preset_at[f] = s; page_at[f] = (f - a) > (b - a) * 2 / 3; }
    }
    std::sort(events.begin(), events.end(), [](const Event& x, const Event& y) { return x.at < y.at || (x.at == y.at && !x.on && y.on); });

    unsigned seed = 99;
    double kick_phase[2] = {0, 0}, kick_time[2] = {1e9, 1e9}; int kick_voice = 0; double snare_t = 1e9, hat_t = 1e9, hat_level = 0, hp = 0, sub_lp = 0;
    double sn_lp = 0, swell_lp = 0;
    // A small stereo room for the drums only (the organ uses its own reverb).
    std::vector<double> dl(4410), dr(5113); size_t dli = 0, dri = 0;
    size_t ev = 0;
    const long drone_off = (long)((steps - 16) * STEP * SR);
    for (long i = 0; i < frames; ++i) {
        const double t = i / SR;
        while (ev < events.size() && events[ev].at <= i) {
            const Event& e = events[ev++];
            if (e.on) voices[e.voice].note_on(e.midi, presets[e.preset].values, presets[e.preset].release, owner++);
            else voices[e.voice].note_off();
        }
        if (i == 0) {
            drone[0].note_on(38, drone_values, 30000, 900);
            drone[1].note_on(45, drone_values, 30000, 901);
        }
        if (i == drone_off) { drone[0].note_off(); drone[1].note_off(); }
        // Organ.
        double organ = 0;
        const int sec = (int)((t / STEP - INTRO) / SECTION);
        const bool in_song = t / STEP >= INTRO && sec < 12;
        double trem = 1;
        if (in_song && tremulant[sec]) trem = 1 - 0.22 * (0.5 + 0.5 * std::sin(2 * M_PI * 4.8 * t));
        for (OrganVoice& v : voices) organ += v.tick();
        organ *= 0.9 * trem;
        const double fade_in = std::fmin(1.0, t / 6.0);
        const double dr0 = drone[0].tick(), dr1 = drone[1].tick();
        organ_bus[i] = (float)(organ + 0.42 * fade_in * (dr0 + 0.6 * dr1));

        // Drums: from the end of the intro to the start of the outro's last bar.
        const double step_f = t / STEP;
        const int step = (int)step_f;
        const bool step_edge = (long)std::floor((i - 1) / SR / STEP) != step || i == 0;
        const bool drums_on = step >= INTRO - 16 && step < steps - 16 && !(in_song && quiet[sec] && (step - INTRO) % SECTION < 32);
        if (step_edge && drums_on) {
            const int s16 = step % 16;
            if (s16 == 0 || s16 == 10 || (s16 == 7 && (step / 16) % 2)) { kick_voice ^= 1; kick_time[kick_voice] = 0; kick_phase[kick_voice] = 0; }
            if (s16 == 8) snare_t = 0;
            if (s16 % 2 == 0) { hat_t = 0; hat_level = (s16 % 4 == 0) ? 0.5 : 0.3; }
        }
        // Triplet hat rolls on the last beat of every other bar.
        static long last_roll = -1;
        if (drums_on && (step / 16) % 2 == 1 && step % 16 >= 12) {
            const long roll = (long)std::floor((step_f - (step - step % 16 + 12)) * 6 / 4) + step / 16 * 100;
            if (roll != last_roll) { last_roll = roll; hat_t = 0; hat_level = 0.2 + 0.03 * (roll % 100); }
        }
        double drums = 0, sub = 0;
        // Two alternating 808 voices: a new hit never cuts the last one off.
        for (int k = 0; k < 2; ++k) {
            double& kt = kick_time[k];
            if (kt >= 3.0) continue;
            const double f = 36.7 + 70 * std::exp(-kt * 28);
            kick_phase[k] += 2 * M_PI * f / SR;
            // A 5 ms fade-in keeps the 808 click-free.
            double one = std::sin(kick_phase[k]) * std::exp(-kt * 1.6) * std::fmin(1.0, kt / 0.005);
            sub += std::tanh(1.6 * one) / std::tanh(1.6);
            kt += 1 / SR;
        }
        if (snare_t < 1.0) {
            const double n = frand(seed);
            sn_lp += 0.45 * (n - sn_lp);
            const double body = std::sin(2 * M_PI * 185 * snare_t) * std::exp(-snare_t * 30);
            drums += (0.55 * (n - sn_lp) * std::exp(-snare_t * 9) + 0.3 * body) * std::fmin(1.0, snare_t / 0.001);
            snare_t += 1 / SR;
        }
        if (hat_t < 0.2) {
            const double n = frand(seed);
            hp = n - hp * 0.0;  // white, then a first difference: bright
            static double prev = 0; const double h = n - prev; prev = n;
            drums += hat_level * 0.35 * h * std::exp(-hat_t * 55) * std::fmin(1.0, hat_t / 0.0005);
            hat_t += 1 / SR;
        }
        // A rising noise swell into each section the drums rejoin.
        double swell = 0;
        if (in_song) {
            const double into = (step_f - INTRO) - sec * SECTION;
            if (quiet[sec] && into >= 24 && into < 32) {
                const double x = (into - 24) / 8.0;
                swell_lp += (0.02 + 0.3 * x) * (frand(seed) - swell_lp);
                swell = 0.12 * x * x * swell_lp;
            }
        }
        sub_lp += 0.08 * (sub - sub_lp);
        smooth_sub[i] = (float)sub_lp;
        // Drum room.
        const double dry = drums + swell;
        const double wl = dl[dli], wr = dr[dri];
        dl[dli] = dry + 0.45 * wr; dr[dri] = dry + 0.45 * wl;
        dli = (dli + 1) % dl.size(); dri = (dri + 1) % dr.size();
        const double master_in = std::fmin(1.0, t / 0.02);
        const double tail = std::fmax(0.0, std::fmin(1.0, (frames / SR - t) / 2.5));
        const double o = organ_bus[i];
        L[i] = (float)(master_in * tail * (0.95 * o + 0.6 * sub_lp + 0.8 * dry + 0.22 * wl));
        R[i] = (float)(master_in * tail * (0.95 * o + 0.6 * sub_lp + 0.8 * dry + 0.22 * wr));
        // Light stereo: the drone a touch left, the figures a touch right.
        L[i] += (float)(0.06 * fade_in * dr0);
        R[i] += (float)(0.06 * organ);
    }
    // Normalize to -1 dBFS peak.
    double peak = 0;
    for (long i = 0; i < frames; ++i) peak = std::fmax(peak, std::fmax(std::fabs(L[i]), std::fabs(R[i])));
    const double g = 0.89 / peak;
    FILE* f = std::fopen("song.f32", "wb");
    for (long i = 0; i < frames; ++i) { float s[2] = {(float)(L[i] * g), (float)(R[i] * g)}; std::fwrite(s, 4, 2, f); }
    std::fclose(f);
    // The smooth stem (organ, drone and sub), for the click check.
    for (long i = 0; i < frames; ++i) organ_bus[i] = (float)(organ_bus[i] + 0.6 * smooth_sub[i]);
    f = std::fopen("organ.f32", "wb");
    std::fwrite(organ_bus.data(), 4, frames, f);
    std::fclose(f);
    f = std::fopen("timeline.jsonl", "w");
    for (long v = 0; v < video_frames; ++v) std::fprintf(f, "[%d,%d,%d]\n", preset_at[v], held[v], page_at[v]);
    std::fclose(f);
    std::printf("%.1f s, peak %.3f, gain %.3f\n", frames / SR, peak, g);
}
