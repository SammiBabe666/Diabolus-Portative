// The machine's display code driven by the song's timeline: the preset's
// controls, the featured track's held notes, and the page.
#include <opxy/draw.hpp>
#include <cstdio>
#include <cstring>
#include "display.cpp"
#include "tests/presets.h"
typedef diabolus::Frame MFrame;
int main() {
    static diabolus::DisplayState state; static MFrame frame;
    std::memset(&state, 0, sizeof state);
    diabolus::display_reset(state);
    int preset, held, page; unsigned serial = 0, off = 0, before = 0;
    while (std::scanf(" [%d,%d,%d]", &preset, &held, &page) == 3) {
        opxy::draw::Input in; std::memset(&in, 0, sizeof in);
        const double* v = presets[preset < 0 ? 0 : preset].values;
        for (int i = 0; i < 8; ++i) in.slots[i] = (int)(v[i] * 2147483647.0);
        for (int c = 0; c < 4; ++c) { in.shown[c] = (unsigned char)(page ? 4 + c : c); in.parameters[c] = in.slots[in.shown[c]]; }
        if (held > (int)before) serial += held - before;
        if (held < (int)before) off += before - held;
        before = held;
        in.elapsed = 1024 / 25; in.track = 1; in.note_serial = serial; in.note_off_serial = off; in.held_notes = held;
        diabolus::display(state, in, frame);
        if (!opxy::draw::valid(frame)) return 1;
        std::printf("{\"p\":%d,\"n\":[", preset);
        for (unsigned i = 0; i < MFrame::node_count; ++i) {
            const opxy::draw::Node& n = frame.nodes[i];
            std::printf("%s[%d,%d,%d,%d,%d,%u,%u]", i ? "," : "", n.x, n.y, n.scale, n.cos, n.sin, n.shade, (unsigned)n.paint);
        }
        std::printf("],\"t\":[");
        for (unsigned i = 0; i < MFrame::text_count; ++i) {
            const opxy::draw::Text& x = frame.texts[i];
            std::printf("%s[%d,%d,%d,\"%s\"]", i ? "," : "", x.x, x.y, x.scale, x.chars);
        }
        std::printf("]}\n");
    }
}
