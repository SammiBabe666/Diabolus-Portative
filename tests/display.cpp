// The screen: every frame valid for any input, captions that say what the
// sound does, knobs that follow STOPS, and the floating nun and candles that
// rise only while notes are held, settle exactly back, and keep still under
// reduced motion.
#include <opxy/draw.hpp>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "../display.cpp"

typedef diabolus::Frame MFrame;
using diabolus::u32;
using diabolus::i32;

static void require(bool ok, const char* message) {
    if (!ok) {
        std::fprintf(stderr, "FAIL %s\n", message);
        std::exit(1);
    }
}

static diabolus::DisplayState state;
static MFrame frame, rest;

static opxy::draw::Input input(const double (&v)[8], u32 held, bool page2, u32 reduced) {
    opxy::draw::Input in;
    std::memset(&in, 0, sizeof in);
    for (int i = 0; i < 8; ++i) in.slots[i] = static_cast<i32>(v[i] * 2147483647.0);
    for (int c = 0; c < 4; ++c) {
        in.shown[c] = static_cast<opxy::u8>(page2 ? 4 + c : c);
        in.parameters[c] = in.slots[in.shown[c]];
    }
    in.elapsed = 41;  // 25 frames a second
    in.track = 1;
    in.held_notes = held;
    in.reduced_motion = reduced;
    return in;
}

static void run(const opxy::draw::Input& in, int frames) {
    for (int f = 0; f < frames; ++f) {
        diabolus::display(state, in, frame);
        require(opxy::draw::valid(frame), "every frame is valid");
    }
}

static bool moved(u32 first, u32 count) {
    return std::memcmp(&frame.nodes[first], &rest.nodes[first], count * sizeof(opxy::draw::Node)) != 0;
}

int main() {
    const double v[8] = {0.3, 0.5, 0.3, 0, 0.15, 0.3, 0, 0.3};
    std::memset(&state, 0, sizeof state);
    diabolus::display_reset(state);
    run(input(v, 0, false, 0), 50);
    rest = frame;

    // Notes held: the nun and every candle leave their places within a second.
    run(input(v, 3, false, 0), 25);
    require(moved(diabolus::nun_node, diabolus::nun_count), "the nun floats while notes are held");
    for (u32 j = 0; j < diabolus::candle_count; ++j) {
        require(moved(diabolus::candle_node + j, 1), "every candle floats while notes are held");
        require(frame.nodes[diabolus::candle_node + j].y < rest.nodes[diabolus::candle_node + j].y - 8 * 1024,
                "candles rise well clear of their prickets");
    }
    // The nun moves as one rigid figure: every part shares one pose.
    for (u32 j = 1; j < diabolus::nun_count; ++j)
        require(std::memcmp(&frame.nodes[diabolus::nun_node], &frame.nodes[diabolus::nun_node + j],
                            sizeof(opxy::draw::Node)) == 0, "the nun's parts move together");
    // Released: everything settles exactly back within a second.
    run(input(v, 0, false, 0), 25);
    require(!moved(diabolus::candle_node, diabolus::nun_node + diabolus::nun_count - diabolus::candle_node),
            "the figures settle exactly back to rest");

    // Reduced motion: holding notes moves nothing but the breath.
    run(input(v, 3, false, 1), 50);
    require(!moved(diabolus::candle_node, diabolus::nun_node + diabolus::nun_count - diabolus::candle_node),
            "reduced motion keeps the figures still");

    // The redraw budget: nothing changes at rest; while notes are held, about
    // 20 changed nodes a frame on average at most (docs/ux.md, Screens).
    {
        static const double heavy[8] = {1, 1, 1, 1, 1, 1, 1, 1};
        run(input(heavy, 0, false, 0), 50);
        MFrame previous = frame;
        run(input(heavy, 0, false, 0), 1);
        require(std::memcmp(&previous, &frame, sizeof frame) == 0, "an idle screen is still");
        u32 changed = 0, frames = 0, peak = 0;
        for (int f = 0; f < 250; ++f) {
            previous = frame;
            run(input(heavy, 3, false, 0), 1);
            u32 now = 0;
            for (u32 i = 0; i < MFrame::node_count; ++i)
                now += std::memcmp(&previous.nodes[i], &frame.nodes[i], sizeof(opxy::draw::Node)) != 0;
            changed += now;
            peak = now > peak ? now : peak;
            ++frames;
        }
        std::printf("changed nodes per frame while held: average %.1f, peak %u\n", double(changed) / frames, peak);
        require(changed <= 20 * frames, "the redraw stays within about 20 nodes a frame");
        run(input(heavy, 0, false, 0), 50);
    }

    // Every control reads: at 0, 50 and 100 each one changes the drawing.
    for (int c = 0; c < 8; ++c) {
        double s[8] = {0.3, 0.5, 0.3, 0, 0.15, 0.3, 0.5, 0.5};
        MFrame at[3];
        for (int k = 0; k < 3; ++k) {
            s[c] = k / 2.0;
            run(input(s, 0, false, 0), 1);
            at[k] = frame;
        }
        bool differs = true;
        for (int k = 0; k < 2; ++k) {
            bool any = false;
            for (u32 i = diabolus::knob_node; i < MFrame::node_count; ++i)
                any |= std::memcmp(&at[k].nodes[i], &at[k + 1].nodes[i], sizeof(opxy::draw::Node)) != 0;
            differs &= any;
        }
        if (!differs) std::printf("control %d\n", c);
        require(differs, "every control visibly changes the screen at 0, 50 and 100");
    }

    // Knobs follow STOPS: none drawn beyond 8' at zero, all eight at full.
    {
        double s[8] = {0, 0.5, 0.3, 0, 0.15, 0.3, 0, 0.3};
        run(input(s, 0, false, 0), 1);
        u32 filled = 0;
        for (u32 j = 0; j < diabolus::knob_count; ++j)
            filled += frame.nodes[diabolus::knob_node + j].shade == diabolus::ink_white;
        require(filled == 1, "STOPS at zero draws only the 8' rank");
        s[0] = 1;
        run(input(s, 0, false, 0), 1);
        filled = 0;
        for (u32 j = 0; j < diabolus::knob_count; ++j)
            filled += frame.nodes[diabolus::knob_node + j].shade == diabolus::ink_white;
        require(filled == diabolus::knob_count, "STOPS at full draws every rank");
    }

    // Captions: ATTACK in time, FEEDBACK as its real 0..85 percent.
    {
        char text[opxy::draw::header_capacity];
        diabolus::attack_caption(text, sizeof text, 0);
        require(std::strcmp(text, "3ms") == 0, "ATTACK at zero reads 3ms");
        diabolus::attack_caption(text, sizeof text, 2147483647);
        require(std::strcmp(text, "1.5s") == 0, "ATTACK at full reads 1.5s");
        for (i32 v = 0; v < 2147483647 - 4194304; v += 4194304) {
            diabolus::attack_caption(text, sizeof text, v);
            require(std::strlen(text) <= 5, "ATTACK captions fit their column");
        }
        diabolus::attack_caption(text, sizeof text, 1258291200);  // about 0.3 s
        require(text[0] == '0' && text[1] == '.' && text[4] == 's', "ATTACK reads 0.NNs under a second");
        diabolus::feedback_caption(text, sizeof text, 2147483647);
        require(std::strcmp(text, "85") == 0, "FEEDBACK at full reads 85");
        diabolus::feedback_caption(text, sizeof text, -5);
        require(std::strcmp(text, "0") == 0, "negative FEEDBACK reads 0");
    }

    // Hostile input: any values, late or negative frame times, unknown held counts.
    u32 seed = 7;
    for (int f = 0; f < 20000; ++f) {
        opxy::draw::Input in;
        unsigned char* bytes = reinterpret_cast<unsigned char*>(&in);
        for (u32 i = 0; i < sizeof in; ++i) bytes[i] = static_cast<unsigned char>((seed = seed * 1664525u + 1013904223u) >> 24);
        for (int c = 0; c < 4; ++c) in.shown[c] = static_cast<opxy::u8>(in.shown[c] % 8);
        if (f % 3) in.held_notes %= 4;
        diabolus::display(state, in, frame);
        require(opxy::draw::valid(frame), "every frame is valid under hostile input");
    }
    std::printf("display ok\n");
    return 0;
}
