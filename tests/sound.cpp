// What each of the eight controls promises, measured on one held note.
#include "check.h"

static const unsigned second = 44100;
static double buffer[8 * 44100];

// Harmonic levels of A3 (220 Hz) after the attack, with the effects off.
struct Spectrum { double sub, h[9]; };
static Spectrum spectrum(double stops, double body, double bite, double drift = 0) {
    static Player p;
    const double v[8] = {stops, body, bite, drift, 0, 0, 0, 0};
    p.begin(v, 57);
    p.render(buffer, second);
    Spectrum s;
    const double* tail = buffer + second / 2;
    s.sub = magnitude(tail, second / 2, 110);
    for (int k = 1; k <= 8; ++k) s.h[k] = magnitude(tail, second / 2, 220.0 * k);
    s.h[0] = 0;
    return s;
}
static double upper(const Spectrum& s) { return s.h[4] + s.h[5] + s.h[6] + s.h[7] + s.h[8]; }

int main() {
    // Pitch: the fundamental is A3 and nothing sits between the harmonics.
    const Spectrum plain = spectrum(0.3, 0.5, 0.3);
    require(plain.h[1] > 0.3, "the 8' fundamental is strong");
    std::printf("plain: h1 %.3f h2 %.3f h3 %.3f sub %.4f\n", plain.h[1], plain.h[2], plain.h[3], plain.sub);
    std::printf("between harmonics (330.5 Hz) %.5f\n", magnitude(buffer + second / 2, second / 2, 330.5));
    require(magnitude(buffer + second / 2, second / 2, 330.5) < 0.01 * plain.h[1], "energy is at the harmonics");

    // STOPS: from a fundamental-rich principal to a full registration.
    const Spectrum closed = spectrum(0, 0.5, 0.3), full = spectrum(1, 0.5, 0.3);
    std::printf("stops 0 upper %.4f, stops 1 upper %.4f\n", upper(closed), upper(full));
    std::printf("stops 0: h1 %.3f h2 %.3f h3 %.3f\n", closed.h[1], closed.h[2], closed.h[3]);
    require(closed.h[1] > 2 * (closed.h[2] + closed.h[3] + upper(closed)), "STOPS 0 is a fundamental-rich principal");
    require(upper(full) > 8 * upper(closed), "STOPS draws the upper ranks in");

    // BODY: hollow and odd (a stopped pipe) to full, with a 16' only in its upper half.
    const Spectrum hollow = spectrum(0.6, 0, 0.3), thick = spectrum(0.6, 1, 0.3);
    std::printf("body 0 even/odd %.3f, body 1 even/odd %.3f, sub %.4f / %.4f\n",
                (hollow.h[2] + hollow.h[4]) / (hollow.h[3] + hollow.h[5]),
                (thick.h[2] + thick.h[4]) / (thick.h[3] + thick.h[5]), hollow.sub, thick.sub);
    require((hollow.h[2] + hollow.h[4]) / (hollow.h[3] + hollow.h[5]) <
                0.5 * (thick.h[2] + thick.h[4]) / (thick.h[3] + thick.h[5]), "BODY 0 is hollow (odd harmonics)");
    require(hollow.sub < 0.002 && thick.sub > 0.05, "BODY's upper half adds the 16'");

    // BITE: from rounded to bright and nasal.
    const Spectrum soft = spectrum(0.5, 0.5, 0), bright = spectrum(0.5, 0.5, 1);
    std::printf("bite 0 upper/h1 %.3f, bite 1 upper/h1 %.3f\n", upper(soft) / soft.h[1], upper(bright) / bright.h[1]);
    require(upper(bright) / bright.h[1] > 4 * upper(soft) / soft.h[1], "BITE brightens the upper partials");
    require(bright.h[3] + bright.h[5] > bright.h[4] + bright.h[6], "BITE's brightness is nasal (odd partials)");

    // DRIFT: off is perfectly steady; on, the partials beat gently, never wildly.
    {
        static Player p;
        const double still[8] = {0.6, 0.5, 0.5, 0, 0, 0, 0, 0}, moving[8] = {0.6, 0.5, 0.5, 1, 0, 0, 0, 0};
        double low = 1e9, high = 0, low_moving = 1e9, high_moving = 0;
        for (int pass = 0; pass < 2; ++pass) {
            p.begin(pass ? moving : still, 57);
            p.render(buffer, second / 2);
            for (int window = 0; window < 40; ++window) {
                p.render(buffer, 4410);
                const double m = magnitude(buffer, 4410, 660);
                double& lo = pass ? low_moving : low;
                double& hi = pass ? high_moving : high;
                lo = std::fmin(lo, m);
                hi = std::fmax(hi, m);
            }
        }
        std::printf("drift 0 h3 swing %.4f, drift 1 h3 swing %.4f\n", (high - low) / high,
                    (high_moving - low_moving) / high_moving);
        require((high - low) / high < 0.01, "DRIFT 0 is steady");
        require((high_moving - low_moving) / high_moving > 0.1, "DRIFT beats audibly at full");
        require((high_moving - low_moving) / high_moving < 0.6, "DRIFT stays restrained");
    }

    // ATTACK: from an immediate pipe to a slow swell.
    for (int pass = 0; pass < 2; ++pass) {
        static Player p;
        const double v[8] = {0.3, 0.5, 0.3, 0, pass ? 1.0 : 0.0, 0, 0, 0};
        p.begin(v, 57);
        p.render(buffer, 2 * second);
        const double settled = rms(buffer + 3 * second / 2, second / 2);
        unsigned reach = 0;
        while (reach + 441 < 2 * second && rms(buffer + reach, 441) < 0.9 * settled) reach += 441;
        std::printf("attack %d reaches 90%% at %.0f ms\n", pass, reach / 44.1);
        require(pass ? reach > 0.5 * second && reach < 1.8 * second : reach <= 0.02 * second,
                pass ? "ATTACK 1 swells over about a second" : "ATTACK 0 speaks at once");
    }

    // REVERB and DELAY: change the note and listen for the old one afterwards.
    // The engine has no note-off (the OP-XY's envelope follows it), so a pitch
    // change within one voice is the cleanest way to hear its tails.
    for (int effect = 0; effect < 4; ++effect) {
        // 0: dry, 1: reverb, 2: delay without feedback, 3: delay with feedback
        static Player p;
        const double reverb = effect == 1 ? 0.8 : 0, delay = effect >= 2 ? 0.8 : 0, feedback = effect == 3 ? 0.8 : 0;
        const double v[8] = {0.2, 0.5, 0.2, 0, 0, reverb, delay, feedback};
        p.begin(v, 57);
        p.render(buffer, second);
        p.pitch = increment(64);  // E4: 220 Hz no longer sounds
        p.render(buffer, 2 * second);
        const double soon = magnitude(buffer + 2205, 4410, 220);        // 50..150 ms
        const double echo1 = magnitude(buffer + 11907, 4410, 220);     // 270..370 ms
        const double echo3 = magnitude(buffer + 40131, 4410, 220);     // 910..1010 ms
        std::printf("effect %d: after 50 ms %.4f, at 320 ms %.4f, at 960 ms %.4f\n", effect, soon, echo1, echo3);
        if (effect == 0) require(soon < 0.002 && echo1 < 0.002, "dry: the old note stops with the change");
        if (effect == 1) require(soon > 0.01, "REVERB: the old note hangs in the room");
        if (effect == 2) require(echo1 > 0.03 && echo3 < 0.01, "DELAY: one echo at 320 ms without FEEDBACK");
        if (effect == 3) require(echo3 > 0.01, "FEEDBACK: the echo repeats");
    }

    // FEEDBACK at its maximum never runs away, even on a note whose harmonics
    // fall on the delay's comb (100 Hz = 32 repeats of 320 ms per second).
    {
        static Player p;
        const double v[8] = {1, 1, 1, 0, 0, 1, 1, 1};
        p.begin(v, 43.35);  // 100.0 Hz
        double early = 0, late = 0;
        for (int s = 0; s < 30; ++s) {
            p.render(buffer, second);
            const double r = rms(buffer, second);
            if (s == 4) early = r;
            if (s == 29) late = r;
        }
        std::printf("feedback 1 at 100 Hz: rms at 5 s %.3f, at 30 s %.3f\n", early, late);
        require(late < 1.1 * early && late < 0.6, "maximum FEEDBACK settles instead of growing");
    }
    return 0;
}
