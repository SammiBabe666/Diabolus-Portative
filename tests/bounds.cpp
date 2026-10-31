// Hostile input: any control word (negative and full scale included), any
// pitch, voices that start on stale memory, and long random play. The output
// stays inside the declared peak (checked on every sample by Player::render),
// never goes silent or sticks, and stale memory changes nothing.
#include "check.h"

static double a[44100], b[44100];
static u32 state = 12345;
static u32 next_random() { return state = state * 1664525u + 1013904223u; }

int main() {
    // Raw control words across the whole Q31 range, changing every block, with
    // wild pitches: zero, tiny, near Nyquist, the largest increments.
    {
        static Player p;
        const double rest[8] = {0.5, 0.5, 0.5, 0.5, 0.2, 0.5, 0.5, 0.5};
        p.begin(rest, 60);
        const u32 pitches[6] = {0u, 1u, increment(0), increment(127), 0x7fffffffu, 0xffffffffu};
        for (unsigned block = 0; block < 20000; ++block) {
            for (int c = 0; c < 8; ++c) {
                const u32 r = next_random();
                p.controls[c] = (r & 3) == 0 ? static_cast<i32>(0x80000000u) : (r & 3) == 1 ? 0x7fffffff
                                                                                             : static_cast<i32>(r);
            }
            p.pitch = (block & 63) == 0 ? pitches[next_random() % 6] : next_random();
            if ((block & 255) == 0) diabolus::start(p.voice, p.shared, 1);
            p.render(nullptr, opxy::block_frames);
        }
        // After the abuse, a normal note still plays at its normal level.
        static Player fresh;
        fresh.begin(rest, 60);
        fresh.render(nullptr, 44100);
        fresh.render(a, 22050);
        p.set(rest);
        p.pitch = increment(60);
        p.render(nullptr, 88200);
        p.render(b, 22050);
        const double ra = rms(a, 22050), rb = rms(b, 22050);
        std::printf("after abuse rms %.4f, fresh %.4f\n", rb, ra);
        require(rb > 0.8 * ra && rb < 1.25 * ra, "a voice recovers from hostile input");
    }

    // Negative control words read as zero.
    {
        static Player p, q;
        const double zero[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        p.begin(zero, 57);
        q.begin(zero, 57);
        for (int c = 0; c < 8; ++c) q.controls[c] = -2147483647 - 1;
        p.render(a, 44100);
        q.render(b, 44100);
        require(std::memcmp(a, b, sizeof a) == 0, "negative controls act as zero");
    }

    // Every note from the bottom to the top of the keyboard is bounded and audible.
    for (int note = 0; note <= 127; note += 7) {
        static Player p;
        const double loud[8] = {1, 1, 1, 1, 0, 1, 1, 1};
        p.begin(loud, note);
        p.render(a, 44100);
        require(rms(a + 22050, 22050) > 0.005, "every note sounds");
    }

    // Stale memory: a voice left full of garbage plays exactly like a zeroed one.
    {
        static Player p, q;
        const double values[8] = {0.7, 0.6, 0.5, 0.4, 0.1, 0.8, 0.6, 0.7};
        p.begin(values, 64, 3);
        std::memset(&q.voice, 0xA5, sizeof q.voice);
        q.set(values);
        q.pitch = p.pitch;
        diabolus::initialize(q.voice, q.shared, 3);
        diabolus::start(q.voice, q.shared, 3);
        p.render(a, 44100);
        q.render(b, 44100);
        require(std::memcmp(a, b, sizeof a) == 0, "stale voice memory changes nothing");
    }

    // A new note in a sounding voice (a steal) keeps going without a gap.
    {
        static Player p;
        const double values[8] = {0.5, 0.5, 0.5, 0.2, 0, 0.5, 0.3, 0.3};
        p.begin(values, 60);
        p.render(nullptr, 22050);
        diabolus::start(p.voice, p.shared, 1);
        p.pitch = increment(67);
        p.render(a, 4410);
        double low = 1;
        for (unsigned w = 0; w + 441 <= 4410; w += 441) low = std::fmin(low, rms(a + w, 441));
        require(low > 0.02, "a stolen voice does not drop out");
    }
    std::printf("bounds ok\n");
    return 0;
}
