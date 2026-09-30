#!/usr/bin/env python3
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------
#
# Regenerate fs/usr/share/kdos/logo.txt — the fastfetch banner: the KDOS
# wordmark with the tux mascot underneath.
#
# The mascot is NOT hand-drawn: it is decoded from penguin.h in this same
# directory, which is kdos.png cropped and quantised, so the terminal banner,
# the boot splash and the mascot file are the same penguin. Only the
# resolution differs.
#
# Constraints on the output, each one measured on a booted console rather than
# assumed:
#   * The console font is ter-kdos32n (xos4-2 plus six hand-patched box
#     chars). It has U+2588 FULL BLOCK, the double-line box drawing the
#     wordmark needs, AND the shades ░ U+2591 / ▒ U+2592 — grep uni/xos4-2.uni:
#     what it lacks is ▓ U+2593 and the half blocks ▀ ▄. This file used to say
#     it had no shades at all and so drew every cell as a full block, which is
#     why the mascot came out a flat silhouette: with three densities the GLYPH
#     carries coverage and the COLOUR carries material, and a 34x18 grid gets
#     soft edges instead of a staircase.
#   * Character cells are twice as tall as they are wide, so the sampling grid
#     is ~2x wider than tall or the penguin comes out stretched into a pin.
#   * Colours stay in the 16-colour ANSI set, and the slots are /etc/vtrgb's
#     (kdos-getty loads it), not a guess: 0 is the near-black ground, 8 a DARK
#     green, 2 the accent #39ff14, 15 the near-white pale, 3 the amber. The
#     dark slot is what lets a black-bodied penguin be black and still visible.
#   * The bright half of the palette is reachable. ter-kdos32n has 512 glyphs,
#     so fbcon needs a ninth glyph bit — and it takes it from BLINK, not from
#     the foreground intensity. Verified on tty1: `1;37` renders a white block,
#     not glyph+256, and λ (a high codepoint) draws correctly at the same time.
#
#   genlogo.py [outfile]        default: fs/usr/share/kdos/logo.txt
#   genlogo.py --preview out.png    render at true cell aspect to check it

import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_OUT = os.path.normpath(
    os.path.join(HERE, '..', '..', '..', 'fs', 'usr', 'share', 'kdos', 'logo.txt'))

# Mascot sampling grid, in character cells.
#
# Height is the binding constraint, and it is not about the screen: fastfetch
# paints the LOGO block first, then moves the cursor back up to write the info
# column beside it. If the logo is taller than the info column the cursor ends
# up inside the logo, and the shell prompt is then drawn on top of the mascot.
# So the whole logo (6 wordmark + 1 blank + mascot = 25 lines) must stay at or
# under the info column's height. The fastfetch config keeps the info side at
# 26+ lines (tagline plus a trailing blank) to guarantee it wins; dropping the
# mascot to 14 rows to fit instead just turns him into an eyeless blob.
#
# Width follows from height: cells are twice as tall as wide, so ~2x the row
# count keeps the penguin's proportions instead of squashing him.
COLS, ROWS = 34, 18
# A cell wins the rim colour on a minority of bright pixels — the rim light is
# one or two pixels wide in the source and the eyes are smaller still, so
# plain majority sampling erases both.
RIM_BIAS = 0.22

DARK = '\033[1;30m'          # vtrgb slot 8  (18,64,31)   — the black body
ACCENT = '\033[0;32m'        # vtrgb slot 2  (57,255,20)  — the glow, and its rim
LIT = '\033[1;32m'           # vtrgb slot 10 (125,255,92) — the rim's hot edge
PALE = '\033[1;37m'          # vtrgb slot 15 (232,255,238)— belly, eyes
AMBER = '\033[0;33m'         # vtrgb slot 3  (255,176,0)  — beak and feet
FAINT = '\033[2;32m'         # tagline
OFF = '\033[0m'

# penguin.h palette: 0 transparent, 1 black, 2 pale, 3 amber, 4 phosphor,
# 5 phosphor dim.
#
# The body used to share the glow's colour, and that ONE line is why the mascot
# read as a green blob for a release: 186 of its 273 cells came out the same
# green, so the silhouette, the arms and the head had no edges between them.
# kdos.png is a BLACK penguin inside a green halo, and that is what these five
# entries now say — dark body, bright ring, fading halo, white belly, amber
# beak. Nothing here is a new artistic choice; it is the source image's own
# structure, which the old mapping threw away.
INK = {1: DARK, 2: PALE, 3: AMBER, 4: LIT, 5: ACCENT}
PREVIEW_RGB = {                      # /etc/vtrgb, so the preview cannot lie
    DARK: (18, 64, 31),
    ACCENT: (57, 255, 20),
    LIT: (125, 255, 92),
    PALE: (232, 255, 238),
    AMBER: (255, 176, 0),
}
GROUND = (0, 10, 3)                  # vtrgb slot 0

# One glyph per coverage band. A cell that is a third ink is ░, not a full
# block that swallows a third of a penguin's outline into a staircase.
SHADES = ((0.42, '░'), (0.78, '▒'), (1.01, '█'))
# Below this the cell is background. It used to be 0.5 — a full half of a cell
# had to be ink before anything was drawn at all, which is what ate the thin
# rim and the feet.
INK_FLOOR = 0.22

# Unchanged on the wire: LIT is the `1;32` these rows always carried and
# ACCENT the `0;32`, only under names that now say which vtrgb slot they are.
WORDMARK = [
    (LIT, '██╗  ██╗██████╗  ██████╗ ███████╗'),
    (LIT, '██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝'),
    (LIT, '█████╔╝ ██║  ██║██║   ██║███████╗'),
    (ACCENT, '██╔═██╗ ██║  ██║██║   ██║╚════██║'),
    (ACCENT, '██║  ██╗██████╔╝╚██████╔╝███████║'),
    (ACCENT, '╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝'),
]

TAGLINE = [
    "KD's Homebrew Linux Distro",
    'musl · toybox · wlroots · no systemd',
]


def load_penguin():
    """Decode penguin.h's row-major RLE into a list of index rows."""
    src = open(os.path.join(HERE, 'penguin.h')).read()
    w = int(re.search(r'#define PENGUIN_W\s+(\d+)', src).group(1))
    h = int(re.search(r'#define PENGUIN_H\s+(\d+)', src).group(1))
    body = src.split('penguin_rle[] = {', 1)[1].split('};', 1)[0]
    nums = [int(n) for n in re.findall(r'\d+', body)]
    grid = [[0] * w for _ in range(h)]
    x = y = 0
    for i in range(0, len(nums) - 1, 2):
        idx, run = nums[i], nums[i + 1]
        for _ in range(run):
            if y < h and x < w:
                grid[y][x] = idx
            x += 1
            if x >= w:
                x = 0
                y += 1
    return grid, w, h


def sample(grid, w, h):
    """Reduce the bitmap to COLSxROWS cells of (palette index, ink coverage).

    Two separate questions per cell, and keeping them separate is the point:
    WHICH material the cell is (the colour) and HOW MUCH of it is ink (the
    glyph). A cell that is a sliver of a flipper is the same green as the
    middle of the body and must not be drawn as solid as it.
    """
    # Sampled over the WHOLE bitmap, glow included. Cropping to the body's
    # bounding box was tried and reverted: the arms span the full width and the
    # feet the full height, so the crop saves almost nothing, and fitting it to
    # the grid independently in x and y stretched the bird wide. The full frame
    # already carries kdos.png's aspect.
    out = []
    for cy in range(ROWS):
        row = []
        y0, y1 = cy * h // ROWS, max(cy * h // ROWS + 1, (cy + 1) * h // ROWS)
        for cx in range(COLS):
            x0, x1 = cx * w // COLS, max(cx * w // COLS + 1, (cx + 1) * w // COLS)
            counts = {}
            for y in range(y0, y1):
                for x in range(x0, x1):
                    idx = grid[y][x]
                    # 4 and 5 are the halo, and the halo is not sampled: see
                    # halo(). Folding them in here is what produced a body
                    # with bright dashes THROUGH it, because the gaps between
                    # the arms and the chest are exactly where the source's
                    # glow lives.
                    if idx in (4, 5):
                        idx = 0
                    counts[idx] = counts.get(idx, 0) + 1
            total = sum(counts.values())
            solid = total - counts.get(0, 0)
            if solid < INK_FLOOR * total:
                row.append((0, 0.0))
                continue
            frac = solid / total
            # The small features. Amber (beak, feet) is 0.9% of the image and
            # the rim 0.8%: a majority vote erases both every time. The EYES
            # are smaller still and are index 2, which is also the belly — so
            # pale is biased too, and that is what puts eyes in the head
            # instead of a blank green forehead.
            for idx in (3, 2):
                if counts.get(idx, 0) >= RIM_BIAS * total:
                    row.append((idx, frac))
                    break
            else:
                # Otherwise the most common opaque index; ties go to the
                # brighter.
                best = max((c, -i) for i, c in counts.items() if i)
                row.append((-best[1], frac))
        out.append(row)
    return out


def shade(frac):
    for upto, glyph in SHADES:
        if frac < upto:
            return glyph
    return '█'


def render(cells, indent):
    lines = []
    for row in cells:
        line, cur = '', None
        for idx, frac in row:
            if not idx:
                if cur is not None:
                    line += OFF
                    cur = None
                line += ' '
                continue
            col = INK[idx]
            if col != cur:
                line += col
                cur = col
            # Density is the HALO's alone. The body is the silhouette and
            # wants a hard edge — shading its border cells produced a fuzzy
            # boundary that then merged into the ring meant to define it.
            # Belly and beak are flat areas of the drawing and would only
            # speckle.
            line += shade(frac) if idx == 5 else '█'
        if cur is not None:
            line += OFF
        lines.append(' ' * indent + line.rstrip() if not line.strip() else ' ' * indent + line)
    return lines


def visible(line):
    return re.sub(r'\033\[[0-9;]*m', '', line)


def preview(cells, path):
    """Render at true cell aspect, with the shades mixed the way a phosphor
    screen mixes them — a ░ cell really is a quarter of its colour against the
    ground, so the preview shows the fade rather than three flat greens."""
    from PIL import Image
    cw, ch = 8, 16                      # one character cell, true aspect
    im = Image.new('RGB', (COLS * cw, ROWS * ch), GROUND)
    weight = {'░': 0.25, '▒': 0.5, '█': 1.0}
    for y, row in enumerate(cells):
        for x, (idx, frac) in enumerate(row):
            if not idx:
                continue
            fg = PREVIEW_RGB[INK[idx]]
            k = weight[shade(frac) if idx == 5 else '█']
            rgb = tuple(int(GROUND[c] + (fg[c] - GROUND[c]) * k) for c in range(3))
            for j in range(ch):
                for i in range(cw):
                    im.putpixel((x * cw + i, y * ch + j), rgb)
    im = im.resize((im.width * 2, im.height * 2), Image.NEAREST)
    im.save(path)


def halo(cells):
    """Put the glow back as a ring around the finished silhouette.

    kdos.png's halo is one to two PIXELS wide against a 182-pixel penguin. At
    34 columns that is a third of a cell, so sampling it faithfully produced
    what it produced: isolated bright dashes in the gaps between the arms and
    the body, which read as speckle rather than as light. Indices 4 and 5 are
    therefore dropped in sample() and the ring is derived here from the shape
    that survived — still not hand-drawn, still the source image's own
    structure, but at a resolution that can carry it.

    One ring, four-connected, and only into cells that are empty: a halo that
    overwrote the penguin would be eating the thing it is supposed to be
    behind.
    """
    for coverage in (1.0, 0.3):          # ▒ against the body, then ░ outside
        lit = [[bool(idx) for idx, _ in row] for row in cells]
        for y, row in enumerate(cells):
            for x, (idx, _) in enumerate(row):
                if idx:
                    continue
                near = ((y and lit[y - 1][x]) or
                        (y + 1 < ROWS and lit[y + 1][x]) or
                        (x and lit[y][x - 1]) or
                        (x + 1 < COLS and lit[y][x + 1]))
                if near:
                    row[x] = (5, coverage)
    return cells


def main():
    args = sys.argv[1:]
    grid, w, h = load_penguin()
    cells = halo(sample(grid, w, h))

    if args[:1] == ['--preview']:
        preview(cells, args[1])
        print('kdos-logo: preview -> %s' % args[1])
        return

    out = args[0] if args else DEFAULT_OUT
    wordmark_w = max(len(text) for _, text in WORDMARK)
    indent = max(0, (wordmark_w - COLS) // 2)

    # No tagline here: it lives in the fastfetch info column so the logo stays
    # shorter than the info block (see the COLS/ROWS comment).
    lines = ['%s%s%s' % (col, text, OFF) for col, text in WORDMARK]
    lines.append('')
    lines += render(cells, indent)

    with open(out, 'w') as f:
        f.write('\n'.join(lines) + '\n')
    print('kdos-logo: %d lines, %d columns -> %s'
          % (len(lines), max(len(visible(l)) for l in lines), out))


main()
