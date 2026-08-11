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
# Two hard constraints on the output:
#   * The console font is ter-kdos32n (xos4-2 plus six hand-patched box
#     chars). It has U+2588 FULL BLOCK and the double-line box drawing the
#     wordmark needs, but NOT half blocks (▀ ▄) or shades (░ ▒ ▓) — so one
#     cell is one full block, and vertical resolution is what it is.
#   * Character cells are twice as tall as they are wide, so the sampling grid
#     is ~2x wider than tall or the penguin comes out stretched into a pin.
#   * Colours stay in the 16-colour ANSI set: the same file is printed on the
#     TTY (palette from kdos-getty) and in foot, where 2 is the accent green,
#     7 the pale text green and 3 the amber.
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

ACCENT = '\033[1;32m'        # phosphor — the rim light
DIM = '\033[0;32m'           # phosphor dim — the body
PALE = '\033[1;37m'          # pale phosphor — belly and face
AMBER = '\033[1;33m'         # amber — beak and feet
FAINT = '\033[2;32m'         # tagline
OFF = '\033[0m'

# penguin.h palette: 0 transparent, 1 black, 2 pale, 3 amber, 4 phosphor,
# 5 phosphor dim. The black body would be invisible on a black terminal, so it
# maps to the dim phosphor and the rim keeps the bright one.
INK = {1: DIM, 2: PALE, 3: AMBER, 4: ACCENT, 5: DIM}
PREVIEW_RGB = {
    DIM: (0x1f, 0x8f, 0x0c),
    PALE: (0xe8, 0xff, 0xee),
    AMBER: (0xff, 0xb0, 0x00),
    ACCENT: (0x39, 0xff, 0x14),
}

WORDMARK = [
    (ACCENT, '██╗  ██╗██████╗  ██████╗ ███████╗'),
    (ACCENT, '██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝'),
    (ACCENT, '█████╔╝ ██║  ██║██║   ██║███████╗'),
    (DIM, '██╔═██╗ ██║  ██║██║   ██║╚════██║'),
    (DIM, '██║  ██╗██████╔╝╚██████╔╝███████║'),
    (DIM, '╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝'),
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
    """Reduce the bitmap to COLSxROWS cells, each the dominant index present.

    Transparency wins only when it dominates the cell, otherwise the penguin
    loses its thin rim light at this resolution.
    """
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
                    counts[idx] = counts.get(idx, 0) + 1
            total = sum(counts.values())
            solid = total - counts.get(0, 0)
            if solid * 2 < total:
                row.append(0)
                continue
            # Amber (beak, feet) and the bright rim/eyes are small features that
            # a majority vote throws away, so they win on a minority share.
            if counts.get(3, 0) >= RIM_BIAS * total:
                row.append(3)
                continue
            if counts.get(4, 0) >= RIM_BIAS * total:
                row.append(4)
                continue
            # Otherwise the most common opaque index; ties go to the brighter.
            best = max((c, -i) for i, c in counts.items() if i)
            row.append(-best[1])
        out.append(row)
    return out


def render(cells, indent):
    lines = []
    for row in cells:
        line, cur = '', None
        for idx in row:
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
            line += '█'
        if cur is not None:
            line += OFF
        lines.append(' ' * indent + line.rstrip() if not line.strip() else ' ' * indent + line)
    return lines


def visible(line):
    return re.sub(r'\033\[[0-9;]*m', '', line)


def preview(cells, path):
    from PIL import Image
    cw, ch = 8, 16                      # one character cell, true aspect
    im = Image.new('RGB', (COLS * cw, ROWS * ch), (0, 10, 3))
    for y, row in enumerate(cells):
        for x, idx in enumerate(row):
            if not idx:
                continue
            rgb = PREVIEW_RGB[INK[idx]]
            for j in range(ch):
                for i in range(cw):
                    im.putpixel((x * cw + i, y * ch + j), rgb)
    im = im.resize((im.width * 2, im.height * 2), Image.NEAREST)
    im.save(path)


def main():
    args = sys.argv[1:]
    grid, w, h = load_penguin()
    cells = sample(grid, w, h)

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
