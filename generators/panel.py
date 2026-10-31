"""Generates panel.svg.fragment: a nun at a portative organ beneath a
Gothic arch, in white ink on black. Run from the repository root:
    python generators/panel.py .
Every line is a thin closed ribbon (the OP-XY's renderer streaks open or
out-and-back outlines), every number has at most four decimals, and the only
colours are #000000 and #FFFFFF. Bound elements: header rules (nodes 0..7),
stop knobs (8..15), breaths (16..18),
candles (19..23), reverb arcs (24, 25), echoes (26..28), bourdon (29),
bellows (30), horns (31, 32), the nun (33..45), text slots 0..15 and palette 0..2; see display.h."""
import math, os, sys

root = sys.argv[1] if len(sys.argv) > 1 else '.'
W, B = '#FFFFFF', '#000000'
ID = 'diabolus'


def n(v):
    s = ('%.4f' % v).rstrip('0').rstrip('.')
    return '0' if s in ('-0', '') else s


def poly(points):
    out, last = [], None
    for x, y in points:
        p = (round(x, 4), round(y, 4))
        if p != last and (not out or p != out[0]):
            out.append(p)
        last = p
    return 'M' + ' L'.join('%s,%s' % (n(x), n(y)) for x, y in out) + ' Z'


def ribbon(points, width=1.0):
    """A line of the given width as a closed outline: one edge out, the other back."""
    h = width / 2
    left, right = [], []
    for i, (x, y) in enumerate(points):
        a = points[max(i - 1, 0)]
        b = points[min(i + 1, len(points) - 1)]
        dx, dy = b[0] - a[0], b[1] - a[1]
        length = math.hypot(dx, dy) or 1
        nx, ny = -dy / length * h, dx / length * h
        left.append((x + nx, y + ny))
        right.append((x - nx, y - ny))
    return poly(left + right[::-1])


out = []
add = out.append

# While `figure` holds an origin, every outline drawn becomes the next bound
# node, authored relative to that origin so display.cpp can lift and turn
# the whole figure about it as one rigid piece.
figure = None
next_node = [19]


def emit(d_of, points, fill, stroke_width):
    if figure is None:
        add('<path d="%s" fill="%s" stroke="%s" stroke-width="%s"/>' % (d_of(points), fill, W, n(stroke_width)))
        return
    ox, oy = figure
    add('<path id="%s_node_%d" transform="matrix(1 0 0 1 %s %s)" d="%s" fill="%s" stroke="%s" stroke-width="%s"/>' % (
        ID, next_node[0], n(ox), n(oy), d_of([(x - ox, y - oy) for x, y in points]), fill, W, n(stroke_width)))
    next_node[0] += 1


def line(points, width=1.0):
    emit(lambda pts: ribbon(pts, width), points, W, 0.1)


def shape(points, fill=B, width=1.0):
    """A closed outline: black (occluding what is behind) or white, with a white edge."""
    emit(poly, points, fill, width)


def rect(x, y, w, h, fill=B, width=1.0):
    shape([(x, y), (x + w, y), (x + w, y + h), (x, y + h)], fill, width)


def arc(cx, cy, r, a0, a1, steps):
    return [(cx + r * math.cos(math.radians(a0 + (a1 - a0) * i / steps)),
             cy - r * math.sin(math.radians(a0 + (a1 - a0) * i / steps))) for i in range(steps + 1)]


def pointed_arch(left, right, spring, apex, steps=14):
    """Two arcs meeting at a point: the left from its springer to the apex, the right mirrored."""
    mid = (left + right) / 2
    # Centre on the springing line so each arc rises vertically from its springer.
    half, rise = mid - left, spring - apex
    c = (half * half + rise * rise) / (2 * half)  # distance from springer to the arc's centre
    r = c
    cx_left = left + c
    a_top = math.degrees(math.atan2(rise, mid - cx_left))
    left_arc = arc(cx_left, spring, r, 180, a_top, steps)
    right_arc = [(2 * mid - x, y) for x, y in left_arc[::-1]]
    return left_arc + right_arc[1:]


# ---------------------------------------------------------------- the header
add('<g id="%s_panel">' % ID)
add('<!-- SDK two-page header: keep this block before machine artwork below y=38. -->')
add('<path d="M0,0 L480,0 L480,38 L0,38 Z" fill="#000000" stroke="#000000" stroke-width="0.1"/>')
# The rule under the edited row (white on black: no grey anywhere).
for i in range(8):
    add('<path id="%s_node_%d" transform="matrix(1 0 0 1 %d 0)" d="M0,19.2 L60,19.2 L60,20 L0,20 Z" '
        'fill="%s" stroke="%s" stroke-width="0.1"/>' % (ID, i, 60 * i, W, W))
for row, (title_y, value_y, value_size) in enumerate([(10, 17.4, 20), (33, 33, 13)]):
    for col in range(4):
        add('<text id="%s_text_%d" transform="matrix(1 0 0 1 %d %s)" fill="%s" font-family="UniversNextPro-Light" '
            'font-size="10px">-</text>' % (ID, row * 8 + col, 5 + 120 * col, n(title_y), W))
    for col in range(4):
        add('<text id="%s_text_%d" transform="matrix(1 0 0 1 %d %s)" fill="%s" font-family="UniversNextPro-Light" '
            'font-size="%dpx">-</text>' % (ID, row * 8 + 4 + col, 65 + 120 * col, n(value_y), W, value_size))

# ------------------------------------------------------------- the cathedral
FLOOR = 206
# Three bays: a tall pointed arch behind the organ, lower arches either side,
# all springing from slender piers that run down to the floor.
# Fifteen steps a side keeps each ribbon within the device's 64 points.
line(pointed_arch(140, 340, 150, 40, 15), 1.2)
line(pointed_arch(149, 331, 150, 51, 15), 0.6)
for x in (140, 340):
    line([(x, 118), (x, FLOOR)], 1.2)
    rect(x - 5, 146, 10, 4, B, 0.8)
for x in (149, 331):
    line([(x, 150), (x, FLOOR)], 0.6)
for left, right in ((16, 140), (340, 464)):
    line(pointed_arch(left, right, 118, 50, 12), 1.0)
    line(pointed_arch(left + 7, right - 7, 120, 59, 12), 0.5)
for x in (16, 464):
    line([(x, 118), (x, FLOOR)], 1.2)
    rect(x - 5, 114, 10, 4, B, 0.8)
for x in (23, 457):
    line([(x, 120), (x, FLOOR)], 0.5)
# Lancet windows in the far wall, with mullions and a quatrefoil above.
for cx in (68, 412):
    line(pointed_arch(cx - 12, cx + 12, 132, 98, 7) + [(cx + 12, 168), (cx - 12, 168)] +
         [(cx - 12, 132)], 0.7)
    line([(cx, 112), (cx, 168)], 0.5)
    for dx, dy in ((0, -4), (4, 0), (0, 4), (-4, 0)):
        add('<circle cx="%s" cy="%s" r="2.3" fill="%s" stroke="%s" stroke-width="0.5"/>' % (
            n(cx + dx), n(110 + dy - 6), B, W))
# Floor: the edge and stone joints converging toward the nave.
line([(16, FLOOR), (464, FLOOR)], 1.0)
for x0 in range(36, 460, 36):
    x1 = 240 + (x0 - 240) * 1.25
    line([(x0, FLOOR), (x1, 219.5)], 0.4)

# Candles on iron prickets in the side bays. The stands stay put; each candle
# and its flame, one outline about the candle's centre (nodes 19..23), lean
# with DRIFT and lift off to float while keys are held.
CANDLES = [(30, 170), (62, 186), (104, 164), (428, 178), (448, 162)]
CANDLE_W, CANDLE_H = 4.4, 13
for x, top in CANDLES:
    line([(x, top + 1.6), (x, FLOOR - 4)], 0.9)
    shape([(x - 4.5, top), (x + 4.5, top), (x + 2.5, top + 1.8), (x - 2.5, top + 1.8)], B, 0.7)
    shape([(x - 5, FLOOR - 4), (x + 5, FLOOR - 4), (x + 6.5, FLOOR), (x - 6.5, FLOOR)], B, 0.7)
for x, top in CANDLES:
    figure = (x, top - CANDLE_H / 2)
    t0 = top - CANDLE_H
    # Up the right side of the flame from the wick and down its left, with a
    # small flat tip (no turn near straight back).
    right = [(x + 0.4 + 1.9 * math.sin(math.pi * k / 7) * (1 - k / 7.0) ** 0.5, t0 - 0.8 - 1.0 * k) for k in range(1, 7)]
    flame = [(x + 0.5, t0 - 0.3)] + right + [(x + 0.35, t0 - 7.6), (x - 0.35, t0 - 7.6)] + \
            [(2 * x - px, py) for px, py in right[::-1]] + [(x - 0.5, t0 - 0.3)]
    shape([(x - CANDLE_W / 2, top), (x - CANDLE_W / 2, t0 + 0.8), (x - 0.6, t0)] + flame +
          [(x + 0.6, t0), (x + CANDLE_W / 2, t0 + 0.6), (x + CANDLE_W / 2, top)], B, 0.7)
# REVERB fills the vault: two arcs inside the central arch that open out
# from the middle of its springing line (nodes 24, 25).
for k in range(2):
    figure = (240, 150)
    line(pointed_arch(160, 320, 150, 62, 13), 1.0)
# DELAY and FEEDBACK: echoes leaving the organ, three arcs about one centre
# beside the treble pipes (nodes 26..28).
for r in (14, 25, 36):
    figure = (336, 98)
    line([(336 + r * math.cos(math.radians(a)), 98 - r * math.sin(math.radians(a))) for a in range(-32, 33, 8)], 1.4)
figure = None

# ---------------------------------------------------------------- the organ
# Pipes: fourteen square wooden pipes stepping down from the bass end.
PIPES, P_LEFT, P_STEP, P_WIDTH, FOOT = 14, 228, 11.6, 8.4, 148
heights = [round(70 * (16 / 70) ** (i / (PIPES - 1)), 1) for i in range(PIPES)]
for i, h in enumerate(heights):
    x = P_LEFT + P_STEP * i
    top = FOOT - h
    rect(x, top, P_WIDTH, h, B, 0.9)
    # Cap block and the mouth: a small cut-up near the foot.
    line([(x, top + 2.2), (x + P_WIDTH, top + 2.2)], 0.4)
    mouth = FOOT - min(10, h * 0.3)
    shape([(x + 2, mouth), (x + P_WIDTH - 2, mouth), (x + P_WIDTH / 2 + 1, mouth - 2.6),
           (x + P_WIDTH / 2 - 1, mouth - 2.6)], W, 0.3)
# Pipe rail and wind chest with its stop knobs (the knobs are bound nodes).
rect(222, 148, 176, 3, B, 0.8)
rect(220, 151, 180, 9, B, 1.0)
# Keyboard: the key frame, sixteen naturals and their sharps.
shape([(214, 160), (406, 160), (406, 176), (214, 176)], B, 1.0)
line([(214, 164), (406, 164)], 0.5)
K_LEFT, K_STEP = 220, 11.25
for i in range(17):
    line([(K_LEFT + K_STEP * i, 164.5), (K_LEFT + K_STEP * i, 175.5)], 0.5)
for i in range(16):
    if i % 7 in (2, 6):
        continue
    x = K_LEFT + K_STEP * (i + 1) - 2.6
    shape([(x, 164.5), (x + 5.2, 164.5), (x + 5.2, 171), (x, 171)], W, 0.3)
# Cheek pieces rising beside the keys.
shape([(206, 158), (214, 156), (214, 178), (206, 178)], B, 0.9)
shape([(406, 156), (414, 158), (414, 178), (406, 178)], B, 0.9)
# Case: moulding, a carved panel with a small horned mask, and feet.
rect(204, 178, 212, 4, B, 1.0)
rect(208, 182, 204, 21, B, 1.0)
rect(214, 185, 192, 15, B, 0.5)
for side in (-1, 1):
    # Scrolling vine from the mask outward.
    pts = [(310 + side * (14 + t * 3.0), 192.5 - 3.5 * math.sin(t * 1.1)) for t in range(0, 28)]
    line(pts, 0.5)
    for t in (6, 13, 20, 26):
        cx, cy = pts[t]
        add('<circle cx="%s" cy="%s" r="1.4" fill="%s" stroke="%s" stroke-width="0.4"/>' % (n(cx), n(cy + 2.4), B, W))
add('<circle cx="310" cy="192.5" r="4.6" fill="%s" stroke="%s" stroke-width="0.7"/>' % (B, W))
# BITE grows the mask's horns, each from its root (nodes 31, 32): drawn below.
shape([(307.5, 192), (309.2, 191.6), (308.4, 193)], W, 0.2)
shape([(312.5, 192), (310.8, 191.6), (311.6, 193)], W, 0.2)
for x in (212, 402):
    rect(x, 203, 6, 3, B, 0.8)
# BODY raises the 16' bourdon behind the bass pipes, growing from its foot
# (node 29); ATTACK opens the bellows about their hinge (node 30): wide for
# pipes that speak at once, nearly shut for slow speech.
figure = (217.5, 148)
rect(213, 64, 9, 84, B, 0.9)
figure = (222, 151.5)
shape([(196, 150), (204, 148.5), (212, 147), (222, 145), (222, 158), (212, 157.2), (204, 156.6), (196, 156)], B, 0.9)
figure = (306.5, 189.2)
shape([(305.5, 190), (302, 184.5), (307.5, 188.5)], W, 0.3)
figure = (313.5, 189.2)
shape([(314.5, 190), (318, 184.5), (312.5, 188.5)], W, 0.3)
figure = None

# --------------------------------------------------------------- the organist
# A nun seated on a bench, in profile, facing the keys: veil, white wimple
# framing her face, and a plain habit.
# Bench.
rect(140, 177, 52, 4, B, 0.9)
for x in (144, 186):
    line([(x, 181), (x, FLOOR)], 1.0)
# Everything from the habit to the hand is one figure (nodes 29 on), turning
# about her middle when she rises off the bench.
NUN_FIRST = next_node[0]
figure = (176, 150)
# Habit and veil: the veil falls from the crown over the shoulders and back.
robe = [(150, 136), (154, 120), (159, 107), (165, 100), (172, 97), (179, 98), (184, 101), (183, 110),
        (182, 121), (186, 126), (189, 134), (190, 140), (194, 152), (198, 166), (206, 175), (210, 179),
        (205, 182), (204, 196), (208, 205), (186, 205), (180, 190), (170, 182), (158, 182), (148, 183),
        (142, 196), (139, 205), (130, 205), (136, 186), (142, 160)]
shape(robe, B, 1.1)
# Her face in profile: brow, nose, lips and chin.
shape([(184, 104.5), (187, 106), (188, 109.5), (190.6, 112.6), (188.2, 113.6), (188.8, 116), (187.2, 117),
       (187.4, 119.2), (184.6, 121.6), (182.4, 116), (182.6, 108)], B, 0.7)
# The wimple: a white band framing the face, and the bandeau across the brow.
line([(181.6, 103.4), (180.8, 110), (180.8, 117), (182, 122.4), (186.6, 125.6)], 1.7)
line([(181.4, 103.2), (187.6, 105.6)], 1.4)
# The veil's edge where it leaves the wimple and falls behind.
line([(179, 99), (176, 108), (172, 126), (166, 142)], 0.5)
# A small cross on a cord.
line([(184, 127), (187, 138)], 0.4)
line([(187.4, 137), (188.4, 144)], 0.9)
line([(185.6, 139.6), (190.2, 138.8)], 0.9)
# Folds of the robe.
line([(158, 120), (154, 140), (151, 162)], 0.5)
line([(166, 126), (164, 150), (166, 172)], 0.5)
line([(192, 186), (196, 204)], 0.5)
# The arm reaching to the keys, the hand resting on them.
shape([(176, 132), (186, 136), (198, 152), (214, 160), (222, 162), (220, 166), (210, 166), (194, 160),
       (182, 146)], B, 0.9)
shape([(218, 160.5), (227, 161), (229, 164), (219, 165)], W, 0.3)
figure = None
NUN_COUNT = next_node[0] - NUN_FIRST

# ------------------------------------------------------------------ the title
# A black plinth under the floor joints, and the name in small capitals.
add('<path d="M156,209 L324,209 L324,220 L156,220 Z" fill="%s" stroke="%s" stroke-width="0.1"/>' % (B, B))
for x, word in ((163, 'D I A B O L U S'), (243, 'P O R T A T I V E')):
    add('<text transform="matrix(1 0 0 1 %d 218)" fill="%s" font-family="UniversNextPro-Light" '
        'font-size="8px">%s</text>' % (x, W, word))

# ------------------------------------------------------------- bound elements
# Stop knobs (nodes 8..15): placed on the wind chest by display.cpp.
for j in range(8):
    add('<circle id="%s_node_%d" transform="matrix(1 0 0 1 %d 155)" cx="0" cy="0" r="2.4" fill="%s" stroke="%s" '
        'stroke-width="0.7"/>' % (ID, 8 + j, 252 + 20 * j, B, W))
# Breaths (nodes 16..18): short rising wisps, hidden until a note is held.
for j in range(3):
    pts = [(1.6 * math.sin(t / 2.0), -t * 1.0) for t in range(0, 11)]
    add('<path id="%s_node_%d" transform="matrix(1 0 0 1 640 320)" d="%s" fill="%s" stroke="%s" '
        'stroke-width="0.1"/>' % (ID, 16 + j, ribbon(pts, 0.8), W, W))
# Palette (parked off screen): black, white, black outlined in white.
for i, (fill, stroke) in enumerate(((B, B), (W, W), (B, W))):
    add('<circle id="%s_palette_%d" cx="640" cy="0" r="1" fill="%s" stroke="%s" stroke-width="0.7"/>' % (
        ID, i, fill, stroke))
add('</g>')
assert next_node[0] == NUN_FIRST + NUN_COUNT
print('nodes', next_node[0], 'nun', NUN_FIRST, NUN_COUNT)

with open(os.path.join(root, 'panel.svg.fragment'), 'w') as f:
    f.write('\n'.join(out) + '\n')
