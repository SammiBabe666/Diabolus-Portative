// No clicks, no gaps: a held note stays smooth while any control jumps from
// one end to the other, a note starts from silence, and pitch bends glide.
// A click is a sample-to-sample step far larger than the sound's own.
#include "check.h"

static double before[22050], during[2205], after[22050];

// Largest second difference: steady tones keep it small, a step makes it spike.
static double roughness(const double* x, unsigned n) {
    double m = 0;
    for (unsigned i = 2; i < n; ++i) m = std::fmax(m, std::fabs(x[i] - 2 * x[i - 1] + x[i - 2]));
    return m;
}

int main() {
    // Every control jumping between 0 and 1 on a sustained note, with the
    // other controls at several settings.
    const double bases[3][8] = {{0.3, 0.5, 0.3, 0, 0.6, 0.3, 0.3, 0.3},
                                {1, 1, 1, 0.5, 0.6, 1, 1, 1},
                                {0, 0, 0, 0, 0.6, 0, 0, 0}};
    double worst = 0;
    for (int base = 0; base < 3; ++base)
        for (int control = 0; control < 8; ++control)
            for (int direction = 0; direction < 2; ++direction) {
                static Player p;
                double values[8];
                std::memcpy(values, bases[base], sizeof values);
                values[control] = direction;
                p.begin(values, 52);
                p.render(nullptr, 88200);            // two seconds to settle
                p.render(before, 22050);
                values[control] = 1 - direction;
                p.set(values);
                p.render(during, 2205);              // the 50 ms after the jump
                p.render(nullptr, 44100);
                p.render(after, 22050);
                const double steady = std::fmax(roughness(before, 22050), roughness(after, 22050));
                const double jump = roughness(during, 2205);
                worst = std::fmax(worst, jump / (steady + 1e-4));
                if (jump > 1.3 * steady + 0.002)
                    std::printf("control %d base %d direction %d: steady %.4f jump %.4f\n", control, base,
                                direction, steady, jump);
                require(jump <= 1.3 * steady + 0.002, "a control change clicks");
            }
    std::printf("worst control jump roughness ratio %.2f\n", worst);

    // A note starts from silence, chiff and all, for every attack.
    for (int a = 0; a <= 4; ++a) {
        static Player p;
        const double values[8] = {0.6, 0.5, 1, 0.5, a / 4.0, 0.5, 0.5, 0.5};
        p.begin(values, 60);
        p.render(during, 2205);
        require(std::fabs(during[0]) < 0.01 && std::fabs(during[1]) < 0.02, "a note starts from silence");
        p.render(nullptr, 44100);
        p.render(before, 22050);
        // The chiff is a designed breath of differentiated noise, as rough as a
        // bright tone at most, and it fades in over about a millisecond. Fast
        // attacks may add that much roughness, no more; once the chiff is gone
        // (slow attacks) the onset must be smoother than the held tone.
        const double onset = roughness(during, 2205), steady = roughness(before, 22050);
        std::printf("attack %.2f onset roughness %.4f, held %.4f\n", a / 4.0, onset, steady);
        require(onset < steady + 0.3, "the onset does not click");
        if (a >= 3) require(onset < steady, "a slow onset is smoother than the held tone");
        double head = 0, rest = 0;
        for (unsigned i = 0; i < 32; ++i) head = std::fmax(head, std::fabs(during[i]));
        for (unsigned i = 32; i < 2205; ++i) rest = std::fmax(rest, std::fabs(during[i]));
        require(head <= rest, "the onset swells instead of striking");
    }

    // Pitch bends glide: a fast bend over a block is as smooth as the tone.
    {
        static Player p;
        const double values[8] = {0.8, 0.5, 0.6, 0.2, 0, 0.5, 0.5, 0.5};
        p.begin(values, 48);
        p.render(nullptr, 44100);
        p.render(before, 22050);
        double steady = roughness(before, 22050), bent = 0;
        for (int b = 0; b < 200; ++b) {
            p.pitch += 400000;  // about two octaves over 0.3 s
            p.render(during, 64);
            bent = std::fmax(bent, roughness(during, 64));
        }
        p.render(after, 22050);
        steady = std::fmax(steady, roughness(after, 22050));
        require(bent <= 1.3 * steady + 0.002, "a bend clicks");
    }

    // Long holds never drop out: two minutes of a drone stays level.
    {
        static Player p;
        const double values[8] = {0.5, 0.7, 0.4, 0.4, 0.2, 0.7, 0.4, 0.6};
        p.begin(values, 38);
        p.render(nullptr, 88200);
        double low = 1e9, high = 0;
        for (int s = 0; s < 120 * 10; ++s) {
            p.render(before, 4410);
            const double r = rms(before, 4410);
            low = std::fmin(low, r);
            high = std::fmax(high, r);
        }
        std::printf("two-minute drone: rms per 100 ms from %.3f to %.3f\n", low, high);
        require(low > 0.5 * high, "a held drone never dips or drops out");
    }
    return 0;
}
