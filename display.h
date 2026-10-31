#pragma once
#include <opxy/page_header.hpp>

namespace diabolus {
// Matches resources.display in definition.json: paths, points, nodes, texts, palette.
// No mutable outlines. Nodes: the two-page header's eight rules (0..7), the
// eight stop knobs on the wind chest (8..15), three breaths above the pipes
// (16..18), five candles (19..23) and their flames (24..28), and the nun,
// thirteen outlines drawn about one origin (29..41).
using Frame = opxy::draw::Frame<0, 0, 42, 16, 3>;
const opxy::u32 header_node = 0, knob_node = 8, breath_node = 16;
const opxy::u32 candle_node = 19, flame_node = 24, nun_node = 29;
const opxy::u32 knob_count = 8, breath_count = 3, candle_count = 5, nun_count = 13;
// Palette: black, white, and black filled with a white outline.
const opxy::u32 ink_black = 0, ink_white = 1, ink_outline = 2;
const opxy::u32 wobble_count = 5;
struct DisplayState {
    opxy::draw::PageHeader header;   // 20 bytes
    opxy::draw::NoteTracker notes;   // 16 bytes
    opxy::i32 breath;                // Q10: 0 hidden, unit fully risen
    opxy::i32 lift;                  // Q10: 0 at rest, unit floating
    opxy::i32 wobble[wobble_count];  // slow oscillators, tenths of a degree
};
}
