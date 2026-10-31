// Runs the machine's display code over a scripted timeline and prints each
// frame's nodes and texts as JSON lines, for rendering into the README GIF.
#include <opxy/draw.hpp>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "display.cpp"
using diabolus::DisplayState; using diabolus::u32; typedef diabolus::Frame MFrame;
int main(int argc, char** argv) {
    const int fps = 25;
    const double seconds = argc > 1 ? atof(argv[1]) : 12;
    const int reduced = argc > 2 ? atoi(argv[2]) : 0;
    static DisplayState state; static MFrame frame;
    std::memset(&state, 0, sizeof state);
    diabolus::display_reset(state);
    double v[8] = {0.3, 0.5, 0.3, 0, 0.15, 0.3, 0, 0.3};
    u32 serial = 0, off = 0, held_before = 0;
    for (int f = 0; f < seconds * fps; ++f) {
        const double t = f / double(fps);
        opxy::draw::Input in; std::memset(&in, 0, sizeof in);
        // Script: STOPS sweeps up (0.5..3 s), notes held (3.5..7 s), page 2 and
        // REVERB/DELAY turned (7.5..10 s) with a chord held again (8.5..11.5 s).
        if (t >= 0.5 && t < 3) v[0] = 0.0 + (t - 0.5) / 2.5;
        if (t >= 7.8 && t < 9.8) { v[5] = 0.3 + 0.6 * (t - 7.8) / 2; v[6] = 0.4 * (t - 7.8) / 2; }
        const bool page2 = t >= 7.5;
        u32 held = ((t >= 3.5 && t < 7) || (t >= 8.5 && t < 11.5)) ? 3 : 0;
        if (held && !held_before) serial += 3;
        if (!held && held_before) off += 3;
        held_before = held;
        for (int i = 0; i < 8; ++i) in.slots[i] = int(v[i] * 2147483647.0);
        for (int c = 0; c < 4; ++c) { in.shown[c] = page2 ? 4 + c : c; in.parameters[c] = in.slots[in.shown[c]]; }
        in.elapsed = 1024 / fps; in.track = 1; in.note_serial = serial; in.note_off_serial = off;
        in.held_notes = held; in.reduced_motion = reduced;
        diabolus::display(state, in, frame);
        if (!opxy::draw::valid(frame)) { fprintf(stderr, "invalid frame %d\n", f); return 1; }
        printf("{\"n\":[");
        for (u32 i = 0; i < MFrame::node_count; ++i) {
            const opxy::draw::Node& n = frame.nodes[i];
            printf("%s[%d,%d,%d,%d,%d,%u,%u]", i ? "," : "", n.x, n.y, n.scale, n.cos, n.sin, n.shade, (unsigned)n.paint);
        }
        printf("],\"t\":[");
        for (u32 i = 0; i < MFrame::text_count; ++i) {
            const opxy::draw::Text& x = frame.texts[i];
            printf("%s[%d,%d,%d,\"%s\"]", i ? "," : "", x.x, x.y, x.scale, x.chars);
        }
        printf("]}\n");
    }
}
