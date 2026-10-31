#include "display.h"

// The screen: a nun at a portative organ under a Gothic arch,
// white ink on black (panel.svg.fragment, drawn by generators/panel.py).
// Things move only when the player acts: the active header row, the stop
// knobs that STOPS draws, and, while notes are held, breath rising from the
// pipes, the nun lifting off her bench and the candles leaving their prickets
// to float and sway. On release everything settles back exactly to rest.
// Reduced motion keeps all of it still.
namespace diabolus {
namespace {
using opxy::i32;
using opxy::u32;
using namespace opxy::draw;

// Knob j shows the rank of harmonic j + 1 drawn (engine.cpp's kRankStart plus
// half of each rank's entry): 8' always, then 4', 2 2/3', 2', 1 3/5', 1 1/3', 1 1/7', 1'.
const i32 kKnobAt[knob_count] = {0, 3277, 8684, 13435, 19006, 22282, 26215, 29164};
// Knob centres on the wind chest, pixels.
const i32 kKnobX = 252, kKnobStep = 20, kKnobY = 155;
// Breath marks: where each rests above the pipes, pixels.
const i32 kBreathX[breath_count] = {236, 268, 304};
const i32 kBreathY[breath_count] = {76, 90, 104};
// Candle columns and pricket tops, pixels, as panel.py draws them;
// how far each floats and where its sway starts.
const i32 kCandleX[candle_count] = {30, 62, 104, 428, 448};
const i32 kCandleTop[candle_count] = {170, 186, 164, 178, 162};  // pricket tops; centres 6.5 px above
const i32 kCandleRise[candle_count] = {18, 24, 13, 21, 15};
const i32 kCandlePhase[candle_count] = {0, 1700, 900, 2600, 400};
// DRIFT leans the candles at rest, alternately, by up to 9 degrees.
const i32 kCandleLean[candle_count] = {90, -70, 80, -90, 60};
const i32 kNunX = 176, kNunY = 150;
// Origins of the control-drawn parts, pixels, as panel.py draws them.
const i32 kVaultX = 240, kVaultY = 150;      // REVERB arcs
const i32 kEchoX = 336, kEchoY = 98;        // DELAY and FEEDBACK echoes
const i32 kBourdonX = 217, kBourdonY = 148;  // BODY's 16' pipe, at its foot (217.5 px)
const i32 kBellowsX = 222, kBellowsY = 151;  // ATTACK's bellows, at the hinge (151.5 px)
const i32 kHornX[2] = {306, 313}, kHornY = 189;  // BITE's horns, at their roots (+0.5, +0.2 px)
// Oscillator rates, tenths of a degree per second: 0.45, 0.7, 0.95, 0.31 and 3.3 Hz.
const i32 kWobbleRate[wobble_count] = {1620, 2520, 3420, 1116, 11880};
// ATTACK in tenths of a millisecond at seventeen positions (3 ms to 1.5 s).
const i32 kAttackTenths[17] = {30, 44, 65, 96, 142, 209, 308, 455, 671, 989, 1459, 2151, 3172, 4678, 6898,
                               10172, 15000};

// Local copies of the SDK's sine and rotation: an inline SDK helper used by
// two machines in one firmware is defined twice there and fails to link.
i32 wave(i32 tenths) {
    tenths %= 3600;
    if (tenths < 0) tenths += 3600;
    const i32 sign = tenths >= 1800 ? -1 : 1;
    tenths %= 1800;
    if (tenths > 900) tenths = 1800 - tenths;
    const i32 degree = tenths / 10, fraction = tenths % 10;
    const i32 low = sine_q10[degree], high = sine_q10[degree < 90 ? degree + 1 : 90];
    return sign * (low + (high - low) * fraction / 10);
}
Turn spin(i32 tenths) {
    const Turn turned = {wave(tenths + 900), wave(tenths)};
    return turned;
}
// A control as Q10, 0 to unit.
i32 amount(i32 slot) { return slot < 0 ? 0 : (slot >> 16) * unit / 32767; }
// Places a node at a fixed origin (whole pixels plus Q10 extra), scaled and turned.
void pose(Frame& frame, u32 node, i32 x, i32 y, i32 scale, i32 tenths) {
    place(frame, node, x, y, scale, ink_black, keep);
    turn(frame, node, spin(tenths));
}

// Writes a non-negative number into out, returning the next position.
u32 digits(char* out, u32 at, u32 capacity, u32 n) {
    char reversed[10];
    u32 count = 0;
    do {
        reversed[count++] = static_cast<char>('0' + n % 10);
        n /= 10;
    } while (n && count < 10);
    while (count && at + 1 < capacity) out[at++] = reversed[--count];
    out[at] = 0;
    return at;
}

// ATTACK in at most five glyphs, to fit a 20 px header column (about 53 px):
// 3ms to 99ms, then 0.10s to 0.99s, then 1.0s to 1.5s.
void attack_caption(char* out, u32 capacity, i32 value) {
    const i32 q = value < 0 ? 0 : value >> 16;
    const i32 at = q >> 11, fraction = q & 2047;
    const i32 low = kAttackTenths[at], high = kAttackTenths[at < 16 ? at + 1 : 16];
    const u32 ms = static_cast<u32>((low + (high - low) * fraction / 2048 + 5) / 10);
    u32 n;
    if (ms < 100) {
        n = digits(out, 0, capacity, ms);
        if (n + 2 < capacity) { out[n++] = 'm'; out[n++] = 's'; out[n] = 0; }
        return;
    }
    const bool seconds = ms >= 995;
    const u32 shown = seconds ? (ms + 50) / 100 : (ms + 5) / 10;  // tenths of a second, or hundredths
    n = digits(out, 0, capacity, shown / (seconds ? 10 : 100));
    if (n + 4 < capacity) {
        out[n++] = '.';
        if (seconds) {
            out[n++] = static_cast<char>('0' + shown % 10);
        } else {
            out[n++] = static_cast<char>('0' + shown / 10 % 10);
            out[n++] = static_cast<char>('0' + shown % 10);
        }
        out[n++] = 's';
        out[n] = 0;
    }
}

// FEEDBACK's real regeneration: 0 to 85 percent.
void feedback_caption(char* out, u32 capacity, i32 value) {
    const u32 q = value < 0 ? 0 : static_cast<u32>(value) >> 16;
    digits(out, 0, capacity, (q * 85 + 16383) / 32767);
}
}

inline void display_reset(DisplayState& state) {
    state.header.reset();
    state.notes.reset();
    state.breath = 0;
    state.lift = 0;
    for (u32 k = 0; k < wobble_count; ++k) state.wobble[k] = 0;
}

inline void display(DisplayState& state, const Input& input, Frame& frame) {
    const char* const titles[8] = {"Stops", "Body", "Bite", "Drift", "Attack", "Reverb", "Delay", "Feedback"};
    char values[8][header_capacity];
    for (u32 i = 0; i < 8; ++i) header_amount(values[i], header_capacity, input.slots[i]);
    attack_caption(values[4], header_capacity, input.slots[4]);
    feedback_caption(values[7], header_capacity, input.slots[7]);
    page_header(state.header, input, frame, titles, values, header_node);

    // Stops: a filled knob for every rank STOPS has drawn.
    const i32 stops = input.slots[0] < 0 ? 0 : input.slots[0] >> 16;
    for (u32 j = 0; j < knob_count; ++j)
        place(frame, knob_node + j, (kKnobX + kKnobStep * static_cast<i32>(j)) * unit, kKnobY * unit, unit,
              stops >= kKnobAt[j] ? ink_white : ink_outline, fill_and_stroke);

    // Breath: rises over about 0.2 s while notes are held, gone on release.
    state.notes.update(input);
    const bool held = input.held_notes != 0 && input.held_notes <= 128;
    if (input.reduced_motion || !held) {
        state.breath = held ? unit : 0;
    } else {
        const i32 dt = clamp(input.elapsed, 0, unit);
        const i32 pull = clamp(dt * 20, 0, unit);
        state.breath += floor_q10((unit - state.breath) * pull);
        if (unit - state.breath < 8) state.breath = unit;
    }
    for (u32 j = 0; j < breath_count; ++j) {
        if (!state.breath) {
            hide(frame, breath_node + j);
            continue;
        }
        const i32 lift = floor_q10((unit - state.breath) * 8);  // starts 8 px low
        place(frame, breath_node + j, kBreathX[j] * unit, (kBreathY[j] + lift) * unit, unit, ink_white, stroke);
    }

    // What each control does, drawn: these change only when a control turns.
    const i32 body = amount(input.slots[1]), bite = amount(input.slots[2]), drift = amount(input.slots[3]);
    const i32 attack = amount(input.slots[4]), room = amount(input.slots[5]);
    const i32 echo = amount(input.slots[6]), regen = amount(input.slots[7]);
    // BODY: the 16' bourdon rises from a stub to its full 84 px.
    pose(frame, bourdon_node, kBourdonX * unit + unit / 2, kBourdonY * unit, 256 + floor_q10(768 * body), 0);
    // BITE: the mask's horns grow from nubs to twice their size.
    for (u32 j = 0; j < 2; ++j)
        pose(frame, horn_node + j, kHornX[j] * unit + unit / 2, kHornY * unit + 205, 410 + floor_q10(1638 * bite), 0);
    // ATTACK: the bellows stand open 20 degrees for instant speech and close as speech slows.
    pose(frame, bellows_node, kBellowsX * unit, kBellowsY * unit + unit / 2, unit, floor_q10(200 * (unit - attack)));
    // REVERB: two arcs open out to fill the vault; gone when the room is dry.
    for (u32 j = 0; j < 2; ++j) {
        const i32 scale = floor_q10((j ? 600 : 920) * room) + (j ? 130 : 200);
        if (room < 20) hide(frame, vault_node + j);
        else pose(frame, vault_node + j, kVaultX * unit, kVaultY * unit, scale, 0);
    }
    // DELAY sizes the echoes; FEEDBACK lets more of them follow, one after another.
    for (u32 j = 0; j < 3; ++j) {
        const i32 reach = j ? clamp(regen * 3 - static_cast<i32>(j - 1) * unit, 0, unit) : unit;
        const i32 scale = floor_q10((400 + floor_q10(624 * echo)) * reach);
        if (echo < 20 || reach < 40) hide(frame, echo_node + j);
        else pose(frame, echo_node + j, kEchoX * unit, kEchoY * unit, scale > 64 ? scale : 64, 0);
    }

    // Levitation: rises over about a second while notes are held, settles
    // over about half a second after the last release.
    const i32 dt = clamp(input.elapsed, 0, unit / 4);
    if (input.reduced_motion) {
        state.lift = 0;
    } else {
        const i32 target = held ? unit : 0;
        const i32 pull = clamp(dt * (held ? 3 : 6), 0, unit);
        state.lift += floor_q10((target - state.lift) * pull);
        if (state.lift < 4 && !held) state.lift = 0;
        if (unit - state.lift < 4 && held) state.lift = unit;
    }
    for (u32 k = 0; k < wobble_count; ++k)
        state.wobble[k] = (state.wobble[k] + floor_q10(dt * kWobbleRate[k])) % 3600;
    // Eased, so the figures leave and land gently.
    const i32 lift = floor_q10(floor_q10(state.lift * state.lift) * (3 * unit - 2 * state.lift));
    const i32* w = state.wobble;
    // DRIFT also widens the sway: half at zero, all of it at full.
    const i32 sway_depth = 512 + floor_q10(512 * drift);

    // The nun: up to 14 px off the bench, bobbing, drifting and rocking by up to 4 degrees.
    {
        const i32 rise = 11 * unit + 3 * wave(w[0]);           // Q10 px
        const i32 wander = 3 * wave(w[3]) / 2;                  // Q10 px
        const i32 x = kNunX * unit + floor_q10(lift * wander);
        const i32 y = kNunY * unit - floor_q10(lift * rise);
        const i32 rock = floor_q10(floor_q10(lift * sway_depth) * floor_q10(40 * wave(w[2])));
        const Turn turned = spin(rock);
        for (u32 j = 0; j < nun_count; ++j) {
            place(frame, nun_node + j, x, y, unit, ink_black, keep);
            turn(frame, nun_node + j, turned);
        }
    }

    // The candles, each one outline with its flame: they lean with DRIFT at
    // rest, and float to their own heights, bob and sway while notes are held.
    for (u32 j = 0; j < candle_count; ++j) {
        const i32 bob = 3 * wave(w[1] + kCandlePhase[j]);       // Q10 px
        const i32 rise = kCandleRise[j] * unit + bob;
        const i32 x = kCandleX[j] * unit + floor_q10(lift * 2 * wave(w[3] + kCandlePhase[j]));
        const i32 y = kCandleTop[j] * unit - 13 * unit / 2 - floor_q10(lift * rise);
        const i32 lean = floor_q10(kCandleLean[j] * drift);
        const i32 sway = floor_q10(floor_q10(lift * sway_depth) * floor_q10(80 * wave(w[0] + 2 * kCandlePhase[j])));
        pose(frame, candle_node + j, x, y, unit, lean + sway);
    }
}
}
