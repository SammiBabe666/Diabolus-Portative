#include "core.h"

// Diabolus Portative: a portative pipe organ made only of arithmetic.
//
// Nine sine partials (a 16' sub and harmonics 1..8) are mixed like the ranks of
// a small organ. STOPS draws ranks in, BODY trades the hollow, odd-harmonic
// stopped pipe for a full open one, BITE tilts toward nasal upper partials,
// DRIFT adds restrained pipe mistuning, beating and wind wander, and ATTACK
// sets how the wind fills the pipe. Every voice then runs its own dark
// cathedral reverb (a four-line feedback delay network) and a 320 ms delay at
// an eighth of the sample rate. No samples, tables of recorded sound or
// floating point: all integer arithmetic, one division per block at most.
namespace diabolus {
namespace {

inline i32 clamp(i32 value, i32 low, i32 high) { return value < low ? low : value > high ? high : value; }
inline i32 clamp16(i32 value) { return clamp(value, -32767, 32767); }
inline i32 absolute(i32 value) { return value < 0 ? -value : value; }
// The position after `at` in a circular line of `length` steps.
inline u32 next(u32 at, u32 length) { return at + 1 == length ? 0 : at + 1; }

// A sine of a 32-bit phase, Q15 (peaks at +/-32768): a parabola refined once,
// within about 0.1%, so its own harmonics stay near -60 dB.
inline i32 sine(u32 phase) {
    const i32 p = static_cast<i32>(phase >> 16) - ((phase & 0x80000000u) ? 65536 : 0);
    i32 y = (p * (32768 - absolute(p))) >> 13;
    y += (7373 * (((y * absolute(y)) >> 15) - y)) >> 15;
    return y;
}

// A cosine of a 32-bit phase, Q15 (+/-32768), within about 1e-4: an even
// polynomial over each quarter turn. Accurate enough to build eight
// harmonics on (each harmonic multiplies the base's error by its square).
inline i32 cosine(u32 phase) {
    const u32 quadrant = phase >> 30;
    i32 x = static_cast<i32>((phase >> 15) & 0x7fffu);
    if (quadrant & 1) x = 32768 - x;
    const i32 x2 = (x * x + 16384) >> 15;
    i32 y = -629;
    y = 8281 + ((y * x2 + 16384) >> 15);
    y = -40420 + ((y * x2 + 16384) >> 15);
    y = 32768 + ((y * x2 + 16384) >> 15);
    return quadrant == 1 || quadrant == 2 ? -y : y;
}

inline u32 random(u32& seed) {
    seed = seed * 1664525u + 1013904223u;
    return seed;
}

// Linear interpolation between two Q15 values by a Q15 amount.
inline i32 mix(i32 a, i32 b, i32 amount) { return a + (((b - a) * amount) >> 15); }

// Rank tables, Q15, indexed by partial: sub, 8', 4', 2 2/3', 2', 1 3/5', 1 1/3', 1 1/7', 1'.
// STOPS: where each rank starts and finishes entering, its level before and after.
const i32 kRankStart[partial_count] = {0, 0, 0, 4915, 9830, 15729, 19005, 22938, 25559};
// 256 / width of each rank's entry, so no division is needed per block.
const i32 kRankSlope[partial_count] = {0, 0, 1280, 1113, 1164, 1280, 1280, 1280, 1164};
const i32 kRankBase[partial_count] = {0, 32767, 3932, 1311, 655, 164, 164, 0, 82};
const i32 kRankFull[partial_count] = {0, 32767, 20316, 14746, 14746, 9175, 9830, 5243, 8520};
// BITE: soft and bright tilts, and the nasal reed partials it adds on its own.
const i32 kSoft[partial_count] = {32767, 32767, 27853, 19661, 14746, 9830, 7209, 4915, 3932};
const i32 kBright[partial_count] = {26214, 27853, 32767, 44237, 45875, 55706, 52429, 58982, 55706};
const i32 kNasal[partial_count] = {0, 0, 0, 4915, 1638, 6554, 1638, 3932, 1638};
// Highest increment each harmonic may have before it would alias: 0.45 of 44.1 kHz.
const u32 kAliasLimit = 0x73333333u, kAliasFade = 0x63333333u;
const u32 kHarmonicLimit[partial_count] = {0xffffffffu, 0x73333333u, 0x39999999u, 0x26666666u, 0x1ccccccdu,
                                           0x170a3d70u, 0x13333333u, 0x10750750u, 0x0e666666u};
// ATTACK: per-sample envelope increments, Q31, for 3 ms to 1.5 s on an
// exponential turn, at seventeen evenly spaced positions.
const i32 kAttack[17] = {16231925, 11007391, 7464466, 5061895, 3432634, 2327780, 1578542, 1070460, 725914,
                         492265, 333821, 226375, 153512, 104101, 70595, 47872, 32464};
// Reverb line feedback for 2.2 s and 6.5 s decay, Q15; REVERB moves between them.
const i32 kShortDecay[4] = {27621, 26784, 25929, 24816};
const i32 kLongDecay[4] = {30927, 30606, 30272, 29826};

const u32 kSegment = 8;          // samples between level updates
const u32 kRatio = 8;                        // audio samples per effect step
const u32 kSteps = opxy::block_frames / kRatio;  // effect steps per block
const i32 kDryPeak = 11000;      // the summed partials never exceed this (Q15 domain)
const i32 kPrefilter = 8817;     // 2.2 kHz one-pole before the eighth-rate effects
const i32 kEchoDamp = 28557;     // 1.8 kHz loop lowpass at 5512 Hz: each repeat darker
const i32 kLineDamp = 26840;     // 1.5 kHz at 5512 Hz in every reverb line: a dark stone room
const i32 kFeedbackMax = 27853;  // 0.85: regeneration never reaches unity
const i32 kEchoCeiling = 12000;  // echoes above this lower the regeneration
const i32 kReverbOut = 16000;     // reverb network output trim

// A gentle knee above two thirds of half scale, flat at half scale.
inline i32 limit(i32 x) {
    const i32 knee = 10923, top = 16384;
    const i32 a = absolute(x);
    if (a <= knee) return x;
    const i32 t = a - knee;
    const i32 y = t >= 2 * (top - knee) ? top : clamp(knee + t - ((t * t * 3) >> 16), 0, top);
    return x < 0 ? -y : y;
}

// A circular line of N 16-bit samples read and written at one position.
// Until a line has been filled once (Fresh), what it has not yet been
// written reads as silence, so a voice never needs its lines cleared.
template <bool Fresh>
inline i32 tap(const i16* line, u32 at, u32 length, u32 written) {
    return Fresh && written < length ? 0 : line[at];
}

// The effects for one block: 8 steps at 5512 Hz. in holds the decimated dry
// signal, wet receives delay and reverb, already balanced by their gains.
template <bool Fresh>
inline void effects(Voice& v, const i32* in, i32* wet, i32 feedback, const i32 (&decay)[4], i32 echo_gain,
                    i32 echo_step, i32 reverb_gain, i32 reverb_step) {
    u32 delay_at = v.delay_at, d0 = v.diffuser_at[0], d1 = v.diffuser_at[1];
    u32 l0 = v.line_at[0], l1 = v.line_at[1], l2 = v.line_at[2], l3 = v.line_at[3];
    i32 echo_low = v.echo_low, echo_level = v.echo_level;
    i32 low0 = v.line_low[0], low1 = v.line_low[1], low2 = v.line_low[2], low3 = v.line_low[3];
    u32 written = v.written;
    for (u32 s = 0; s < kSteps; ++s) {
        const i32 u = in[s];
        echo_gain += echo_step;
        reverb_gain += reverb_step;
        // Delay: the line holds the input plus damped, bounded regeneration.
        const i32 echo = tap<Fresh>(v.delay, delay_at, delay_length, written);
        echo_low += ((echo - echo_low) * kEchoDamp) >> 15;
        v.delay[delay_at] = static_cast<i16>(clamp16(u + ((echo_low * feedback) >> 15)));
        delay_at = next(delay_at, delay_length);
        const i32 level = absolute(echo);
        echo_level = level > echo_level ? level : echo_level - (echo_level >> 8);
        const i32 echoed = (echo * echo_gain) >> 15;

        // Reverb: the dry signal and its echoes through two allpass diffusers...
        i32 x = (u + echoed) >> 1;
        i32 d = tap<Fresh>(v.diffuser0, d0, diffuser_lengths[0], written);
        i32 w = clamp16(x - ((d * 19661) >> 15));
        v.diffuser0[d0] = static_cast<i16>(w);
        x = d + ((w * 19661) >> 15);
        d0 = next(d0, diffuser_lengths[0]);
        d = tap<Fresh>(v.diffuser1, d1, diffuser_lengths[1], written);
        w = clamp16(x - (d >> 1));
        v.diffuser1[d1] = static_cast<i16>(w);
        x = d + (w >> 1);
        d1 = next(d1, diffuser_lengths[1]);
        // ...into four damped lines mixed by an orthonormal Hadamard matrix,
        // which keeps the network lossless before each line's decay.
        const i32 a0 = tap<Fresh>(v.line0, l0, line_lengths[0], written);
        const i32 a1 = tap<Fresh>(v.line1, l1, line_lengths[1], written);
        const i32 a2 = tap<Fresh>(v.line2, l2, line_lengths[2], written);
        const i32 a3 = tap<Fresh>(v.line3, l3, line_lengths[3], written);
        low0 += ((a0 - low0) * kLineDamp) >> 15;
        low1 += ((a1 - low1) * kLineDamp) >> 15;
        low2 += ((a2 - low2) * kLineDamp) >> 15;
        low3 += ((a3 - low3) * kLineDamp) >> 15;
        const i32 sum01 = low0 + low1, sum23 = low2 + low3, dif01 = low0 - low1, dif23 = low2 - low3;
        v.line0[l0] = static_cast<i16>(clamp16(x + ((((sum01 + sum23) >> 1) * decay[0]) >> 15)));
        v.line1[l1] = static_cast<i16>(clamp16(-x + ((((dif01 + dif23) >> 1) * decay[1]) >> 15)));
        v.line2[l2] = static_cast<i16>(clamp16(x + ((((sum01 - sum23) >> 1) * decay[2]) >> 15)));
        v.line3[l3] = static_cast<i16>(clamp16(-x + ((((dif01 - dif23) >> 1) * decay[3]) >> 15)));
        l0 = next(l0, line_lengths[0]);
        l1 = next(l1, line_lengths[1]);
        l2 = next(l2, line_lengths[2]);
        l3 = next(l3, line_lengths[3]);
        const i32 reverb = (((a0 - a1 + a2 - a3) >> 1) * kReverbOut) >> 14;
        wet[s] = echoed + ((reverb * reverb_gain) >> 15);
        if (Fresh) ++written;
    }
    v.delay_at = static_cast<u16>(delay_at);
    v.diffuser_at[0] = static_cast<u16>(d0);
    v.diffuser_at[1] = static_cast<u16>(d1);
    v.line_at[0] = static_cast<u16>(l0);
    v.line_at[1] = static_cast<u16>(l1);
    v.line_at[2] = static_cast<u16>(l2);
    v.line_at[3] = static_cast<u16>(l3);
    v.echo_low = echo_low;
    v.echo_level = echo_level;
    v.line_low[0] = low0;
    v.line_low[1] = low1;
    v.line_low[2] = low2;
    v.line_low[3] = low3;
    v.written = written;
}
}

// Callbacks run serially per voice; owner is a stable nonzero voice ID.
inline void initialize(Voice& v, Shared&, u32 owner) {
    v.seed = owner * 2654435761u + 0x9e3779b9u;
    v.phase = random(v.seed);  // pipes speak in no particular phase
    for (u32 k = 0; k < partial_count; ++k) {
        v.weight[k] = 0;
        v.beat_phase[k] = random(v.seed);
        // 0.2 to 1.9 Hz, updated once per block (689 Hz)
        v.beat_rate[k] = 1245000u + (random(v.seed) >> 20) * 2580u;
    }
    v.wind_phase[0] = random(v.seed);
    v.wind_phase[1] = random(v.seed);
    for (u32 c = 0; c < control_count; ++c) v.control[c] = 0;
    v.envelope = 0;
    v.chiff = v.chiff_rise = v.noise_low = v.noise_previous = 0;
    v.started = 0;
    v.ready = 0;
    v.prefilter = v.wet_previous = 0;
    v.dry_gain = v.echo_gain = v.reverb_gain = 0;
    v.echo_low = v.echo_level = 0;
    for (u32 j = 0; j < 4; ++j) v.line_low[j] = 0;
    v.written = 0;  // lines read as silence until written: no clearing needed
    v.delay_at = v.diffuser_at[0] = v.diffuser_at[1] = 0;
    for (u32 j = 0; j < 4; ++j) v.line_at[j] = 0;
    v.padding = 0;
}
// A new note in this voice keeps its wind and room; a restarted note speaks
// again with a fresh chiff and continues from its current level, so it never clicks.
inline void start(Voice& v, Shared&, u32) { v.started = 1; }
inline void destroy(Voice&, Shared&, u32) {}

// Controls: Stops, Body, Bite, Drift, Attack, Reverb, Delay, Feedback.
inline void render(Voice& v, Shared&, u32, opxy::Output output, opxy::Parameters parameters,
                   opxy::Pitch pitch) {
    // Controls: bounded, then smoothed over about 11 ms.
    for (u32 c = 0; c < control_count; ++c) {
        const i32 target = parameters[c] < 0 ? 0 : parameters[c];
        v.control[c] = v.ready ? v.control[c] + ((target - v.control[c]) >> 3) : target;
    }
    const i32 stops = v.control[0] >> 16, body = v.control[1] >> 16, bite = v.control[2] >> 16;
    const i32 drift = v.control[3] >> 16, attack = v.control[4] >> 16, room = v.control[5] >> 16;
    const i32 echo = v.control[6] >> 16, regen = v.control[7] >> 16;

    // DRIFT: a squared turn keeps the first half subtle.
    const i32 depth = (drift * drift) >> 15;
    v.wind_phase[0] += 1960000u;   // 0.31 Hz
    v.wind_phase[1] += 5150000u;   // 0.83 Hz
    const i32 wind = (sine(v.wind_phase[0]) * 22938 + sine(v.wind_phase[1]) * 9830) >> 15;
    // Wind pressure bends the whole pipe by up to 3 cents.
    const i32 wander = (((wind * depth) >> 15) * 114) >> 15;
    u32 base = pitch[0];
    base += static_cast<u32>(static_cast<i32>(base >> 16) * wander);

    // Registration: each partial's level from STOPS, BODY and BITE.
    const i32 bite3 = (((bite * bite) >> 15) * bite) >> 15;  // the reeds arrive late in BITE's turn
    i32 target[partial_count];
    i32 sum = 0;
    for (u32 k = 0; k < partial_count; ++k) {
        i32 w;
        if (k == 0) {
            // A 16' bourdon joins in the upper half of BODY.
            const i32 b = clamp((body - 14746) * 2, 0, 32767);
            w = (((b * b) >> 15) * 18022) >> 15;
        } else {
            const i32 into = clamp(((stops - kRankStart[k]) * kRankSlope[k]) >> 8, 0, 32767);
            w = k == 1 ? kRankFull[1] : mix(kRankBase[k], kRankFull[k], into);
            // BODY: hollow (odd partials, a stopped pipe) to full (strong fundamental and evens).
            if (k == 1) w = (w * (22938 + ((body * 14746) >> 15))) >> 15;
            else if ((k & 1) == 0) w = (w * (8192 + ((body * 31130) >> 15))) >> 15;
            else w = (w * (37683 - ((body * 4915) >> 15))) >> 15;
        }
        // BITE: from rounded to bright, plus nasal reed partials of its own.
        w = (w * mix(kSoft[k], kBright[k], bite)) >> 15;
        w += (kNasal[k] * bite3) >> 15;
        // DRIFT: partials beat slowly against the rank, the upper ones more.
        v.beat_phase[k] += v.beat_rate[k];
        const i32 swing = (depth * (k >= 2 ? 13107 : 4915)) >> 15;
        if (swing) {
            // A triangle is smooth enough at these rates once the level is ramped.
            const i32 triangle = absolute(static_cast<i32>(v.beat_phase[k] >> 16) - 32768);  // 0..32768
            w = (w * (32767 - ((swing * (triangle - (triangle >> 15))) >> 15))) >> 15;
        }
        // Silent near Nyquist: harmonics fade out between 17.1 and 19.8 kHz.
        if (k > 0) {
            const u32 inc = base > kHarmonicLimit[k] ? kAliasLimit : base * k;
            if (inc >= kAliasLimit) w = 0;
            else if (inc > kAliasFade) w = (w * static_cast<i32>((kAliasLimit - inc) >> 13)) >> 15;
        }
        target[k] = w;
        sum += w;
    }
    // Normalize the registration to a fixed peak: the partials can never sum above kDryPeak.
    const i32 scale = sum > 0 ? (kDryPeak << 15) / sum : 0;
    for (u32 k = 0; k < partial_count; ++k) target[k] = (target[k] * scale) >> 15;
    if (!v.ready)
        for (u32 k = 0; k < partial_count; ++k) v.weight[k] = target[k];

    // Wind: the attack ramp, upper partials speaking a little ahead of the fundamental.
    const i32 at = attack >> 11, fraction = attack & 2047;
    const u32 rise = static_cast<u32>(kAttack[at] + ((kAttack[at < 16 ? at + 1 : 16] - kAttack[at]) >> 11) * fraction)
                     * kSegment;
    if (v.started) {
        v.started = 0;
        // The chiff: a short breath at the pipe mouth, strongest on fast attacks and high BITE.
        const i32 fast = 32767 - attack;
        v.chiff = v.chiff_rise = (((((fast * fast) >> 15) * (11469 + ((bite * 21299) >> 15))) >> 15) * 5500) >> 15;
    }

    // Mix targets: REVERB and DELAY balance wet against dry; FEEDBACK is bounded.
    const i32 room_dry = 32768 - ((27853 * ((room * room) >> 15)) >> 15);
    const i32 dry_target = ((32768 - (((echo * echo) >> 15) >> 1)) * room_dry) >> 15;
    const i32 echo_target = (echo * room_dry) >> 15;
    const i32 reverb_target = room;
    i32 feedback = (regen * kFeedbackMax) >> 15;
    if (v.echo_level > kEchoCeiling) feedback = feedback * kEchoCeiling / v.echo_level;
    i32 decay[4];
    for (u32 j = 0; j < 4; ++j) decay[j] = mix(kShortDecay[j], kLongDecay[j], room);
    if (!v.ready) {
        v.dry_gain = dry_target;
        v.echo_gain = echo_target;
        v.reverb_gain = reverb_target;
        v.ready = 1;
    }
    const i32 dry_step = (dry_target - v.dry_gain) >> 6;
    i32 weight_step[partial_count];
    for (u32 k = 0; k < partial_count; ++k) weight_step[k] = (target[k] - v.weight[k]) >> 3;

    // Pass 1: the pipes. Levels move every eight samples (registration ramps
    // and the attack). One cosine at the 16' pitch gives all nine partials
    // through the double-angle and product identities, a shallow tree of
    // multiplies rather than a chain: the ranks stay phase-locked like real
    // pipes speaking from one wind chest, and cost a single sine.
    i32 x[opxy::block_frames];
    u32 envelope = v.envelope, phase = v.phase;
    const u32 inc = base >> 1;  // one phase, at the 16' pipe's pitch
    for (u32 segment = 0; segment < opxy::block_frames / kSegment; ++segment) {
        i32 w[partial_count];
        for (u32 k = 0; k < partial_count; ++k) w[k] = v.weight[k] += weight_step[k];
        if (envelope < 0x7fffffffu) {
            envelope = envelope > 0x7fffffffu - rise ? 0x7fffffffu : envelope + rise;
            const i32 e = static_cast<i32>(envelope >> 16);
            const i32 e_high = e + (e >> 1) > 32767 ? 32767 : e + (e >> 1);
            const i32 low = (e * (65536 - e)) >> 15, high = (e_high * (65536 - e_high)) >> 15;
            for (u32 k = 0; k < partial_count; ++k) w[k] = (w[k] * (k <= 2 ? low : high)) >> 15;
        }
        for (u32 n = 0; n < kSegment; ++n) {
            phase += inc;
            const i32 c0 = cosine(phase);                    // cos t/2, Q15: the 16'
            const i32 c1 = ((c0 * c0) >> 14) - 32768;        // cos t = 2 cos^2 t/2 - 1: the 8'
            const i32 c2 = ((c1 * c1) >> 14) - 32768;        // cos 2t = 2 cos^2 t - 1
            const i32 c4 = ((c2 * c2) >> 14) - 32768;
            const i32 c3 = ((c1 * c2) >> 14) - c1;           // cos 3t = 2 cos t cos 2t - cos t
            const i32 c8 = ((c4 * c4) >> 14) - 32768;
            const i32 c5 = ((c1 * c4) >> 14) - c3;
            const i32 c6 = ((c2 * c4) >> 14) - c2;
            const i32 c7 = ((c3 * c4) >> 14) - c1;
            x[segment * kSegment + n] = (c0 * w[0] + c1 * w[1] + c2 * w[2] + c3 * w[3] + c4 * w[4] + c5 * w[5] +
                                         c6 * w[6] + c7 * w[7] + c8 * w[8]) >> 15;
        }
    }
    v.envelope = envelope;
    v.phase = phase;
    for (u32 k = 0; k < partial_count; ++k) v.weight[k] = target[k];

    // Pass 2: the chiff, while it lasts.
    // Its level is the difference of two decays: it rises from silence over
    // about a millisecond, then fades over about 25 ms, so it never clicks.
    if (v.chiff > 16) {
        i32 chiff = v.chiff, rise = v.chiff_rise, noise_low = v.noise_low, previous = v.noise_previous;
        for (u32 i = 0; i < opxy::block_frames; ++i) {
            const i32 noise = static_cast<i32>(random(v.seed) >> 18) - 8192;
            noise_low += (noise - noise_low) >> 1;
            x[i] += ((noise_low - previous) * (chiff - rise)) >> 13;
            previous = noise_low;
            chiff -= chiff >> 10;
            rise -= rise >> 5;
        }
        v.chiff = chiff;
        v.chiff_rise = rise;
        v.noise_low = noise_low;
        v.noise_previous = previous;
    }

    // Pass 3: into the eighth-rate effects through a 2.2 kHz lowpass and an
    // eight-sample average, then the effects themselves.
    i32 in[kSteps], wet[kSteps];
    i32 pre = v.prefilter;
    for (u32 s = 0; s < kSteps; ++s) {
        i32 sum = 0;
        for (u32 q = 0; q < kRatio; ++q) {
            pre += ((x[kRatio * s + q] - pre) * kPrefilter) >> 15;
            sum += pre;
        }
        in[s] = sum >> 3;
    }
    v.prefilter = pre;
    const i32 echo_step = (echo_target - v.echo_gain) >> 3, reverb_step = (reverb_target - v.reverb_gain) >> 3;
    if (v.written < delay_length)
        effects<true>(v, in, wet, feedback, decay, v.echo_gain, echo_step, v.reverb_gain, reverb_step);
    else
        effects<false>(v, in, wet, feedback, decay, v.echo_gain, echo_step, v.reverb_gain, reverb_step);

    // Pass 4: dry and wet together, the wet interpolated back to 44.1 kHz.
    // (Each step's wet value is reached at the end of its eight samples.)
    i32 dry = v.dry_gain, previous = v.wet_previous;
    for (u32 s = 0; s < kSteps; ++s) {
        const i32 current = wet[s], slope = (current - previous) >> 3;
        i32 value = previous;
        for (u32 q = 0; q < kRatio; ++q) {
            const u32 i = kRatio * s + q;
            value += slope;
            dry += dry_step;
            output[i] = limit(((x[i] * dry) >> 15) + value) * 65536;
        }
        previous = current;
    }
    v.wet_previous = previous;
    v.dry_gain = dry_target;
    v.echo_gain = echo_target;
    v.reverb_gain = reverb_target;
}
}
