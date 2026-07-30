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
# Generate the KDOS cursor theme: chunky pixel-art phosphor cursors written
# straight in the Xcursor binary format (raw ARGB — no image libraries, no
# xcursorgen, no X client libs). Every shape cosmic-comp's alias table knows
# is provided, so window borders actually show resize arrows.
#
#   gencursors.py <outdir>            outdir gets cursors/* and index.theme
#   gencursors.py <outdir> --preview  also write a PPM contact sheet

import os
import struct
import sys

# PHOSPHOR — keep in sync with the palette in fs/usr/local/bin/kdos.
COL = {
    '#': 0xFF000000,   # outline
    'G': 0xFF39FF14,   # phosphor
    'W': 0xFFB8FFC8,   # pale text-green
    'R': 0xFFFF3131,   # alarm
    'A': 0xFFFFB000,   # amber
    '.': 0x00000000,
}

SIZES = (24, 32, 48, 64, 96)   # up to 96px: 4K/HiDPI at XCURSOR_SIZE=48 x2
GRID = 16
RIM = 0xFFB8FFC8               # pale rim so the dark outline reads on dark UIs

# --------------------------------------------------------------- pixel art
# 16x16 design grids. Hotspots are grid coordinates.

ARROW = """
#...............
##..............
#G#.............
#GG#............
#GGG#...........
#GGGG#..........
#GGGGG#.........
#GGGGGG#........
#GGGGGGG#.......
#GGGGGGGG#......
#GGGGG#####.....
#GG#GG#.........
#G#.#GG#........
##..#GG#........
....#GG#........
.....##.........
"""

POINTER = """
.....##.........
....#GG#........
....#GG#........
....#GG#........
....#GG###......
....#GG#GG###...
.##.#GG#GG#GG#..
#GG##GGGGGGGGG#.
#GGG#GGGGGGGGG#.
.#GG#GGGGGGGGG#.
.#GGGGGGGGGGGG#.
..#GGGGGGGGGGG#.
..#GGGGGGGGGG#..
...#GGGGGGGGG#..
....#GGGGGGG#...
....#########...
"""

TEXT = """
.###..###.......
...#GG#.........
....##..........
....##..........
....##..........
....##..........
....##..........
....##..........
....##..........
....##..........
....##..........
....##..........
....##..........
....##..........
...#GG#.........
.###..###.......
"""

CROSSHAIR = """
.......#........
......#G#.......
......#G#.......
......#G#.......
......#G#.......
......#G#.......
.######G######..
#GGGGGG.GGGGGG#.
.######G######..
......#G#.......
......#G#.......
......#G#.......
......#G#.......
......#G#.......
.......#........
................
"""

MOVE = """
.......#........
......#G#.......
.....#GGG#......
....#GGGGG#.....
......#G#.......
...#..#G#..#....
..#G###G###G#...
.#GGGGGGGGGGG#..
..#G###G###G#...
...#..#G#..#....
......#G#.......
....#GGGGG#.....
.....#GGG#......
......#G#.......
.......#........
................
"""

HDOUBLE = """
................
................
................
................
................
...#........#...
..#G#......#G#..
.#GG########GG#.
#GGGGGGGGGGGGGG#
.#GG########GG#.
..#G#......#G#..
...#........#...
................
................
................
................
"""

DIAG1 = """
................
.######.........
.#GGG#..........
.#GG#...........
.#G#G#..........
.#G#.#G#........
.##...#G#.......
.......#G#......
........#G#.....
.........#G#....
..........#G#...
.......#G#.#G##.
........#G#GG#..
.........#GGG#..
........######..
................
"""

NOTALLOWED = """
.....######.....
...##RRRRRR##...
..#RRR####RRR#..
.#RRR#....#RRR#.
.#RR#....#RRRR#.
#RRR#...#RRR#R#.
#RR#...#RRR#.R#.
#RR#..#RRR#..R#.
#RR#.#RRR#...R#.
#RRR#RRR#...#R#.
.#RRRRR#....#R#.
.#RRRR#....#RR#.
..#RRR####RRR#..
...##RRRRRR##...
.....######.....
................
"""

GRAB = """
................
................
...#.##.##......
..#G#GG#GG##....
..#G#GG#GG#G#...
..#GGGGGGGG#G#..
.##GGGGGGGGGG#..
#GG#GGGGGGGGG#..
#GGGGGGGGGGGG#..
.#GGGGGGGGGGG#..
..#GGGGGGGGG#...
..#GGGGGGGGG#...
...#GGGGGGG#....
....#GGGGGG#....
....########....
................
"""

GRABBING = """
................
................
................
................
................
...##.##.##.....
..#GG#GG#GG##...
.##GGGGGGGG#G#..
#GG#GGGGGGGGG#..
#GGGGGGGGGGGG#..
.#GGGGGGGGGGG#..
..#GGGGGGGGG#...
..#GGGGGGGGG#...
...#GGGGGGG#....
....########....
................
"""

CELL = """
................
................
................
......###.......
......#G#.......
......#G#.......
...####G####....
...#GGG.GGG#....
...####G####....
......#G#.......
......#G#.......
......###.......
................
................
................
................
"""

# Magnifier body shared by zoom-in / zoom-out; the sign is stamped after.
ZOOM = """
...######.......
..#GGGGGG#......
.#GG####GG#.....
#GG#....#GG#....
#G#......#G#....
#G#......#G#....
#G#......#G#....
#GG#....#GG#....
.#GG####GG#.....
..#GGGGGG##.....
...#######GG#...
..........#GG#..
...........#GG#.
............#G#.
.............#..
................
"""

WAIT_RING = ((7, 2), (11, 4), (13, 8), (11, 12), (7, 14), (3, 12), (1, 8), (3, 4))


def parse(art):
    rows = [r for r in art.strip().split('\n')]
    g = [[COL[c] for c in row.ljust(GRID, '.')[:GRID]] for row in rows]
    while len(g) < GRID:
        g.append([0] * GRID)
    return g


def flip_h(g):
    return [list(reversed(r)) for r in g]


def transpose(g):
    return [list(r) for r in zip(*g)]


def blank():
    return [[0] * GRID for _ in range(GRID)]


def stamp(g, overlay, ox, oy):
    g = [r[:] for r in g]
    for y, row in enumerate(parse(overlay)):
        for x, c in enumerate(row):
            if c and 0 <= oy + y < GRID and 0 <= ox + x < GRID:
                g[oy + y][ox + x] = c
    return g


def dot(g, x, y, w, h, col):
    g = [r[:] for r in g]
    for j in range(y, y + h):
        for i in range(x, x + w):
            if 0 <= j < GRID and 0 <= i < GRID:
                g[j][i] = col
    return g


# Small overlays stamped onto the arrow (bottom-right corner).
OV_PLUS = """
.###.
.#G#.
##G##
#GGG#
##G##
.#G#.
.###.
"""

OV_QUESTION = """
.####.
#GGGG#
##..G#
...#G#
..#G##
..#G#.
..##..
..#G#.
..###.
"""

OV_MENU = """
######
#WWWW#
######
#WWWW#
######
#WWWW#
######
"""

OV_LINK = """
.####.
.#GGG#
.##GG#
.#GGG#
##G#G#
#G#.##
##....
"""


def wait_frames():
    frames = []
    for f in range(8):
        g = blank()
        for i, (x, y) in enumerate(WAIT_RING):
            age = (i - f) % 8
            if age == 0:
                g = dot(g, x, y, 2, 2, COL['W'])
            elif age == 7:
                g = dot(g, x, y, 2, 2, COL['G'])
            elif age == 6:
                g = dot(g, x, y, 2, 2, 0xFF1F8F0C)
            else:
                g = dot(g, x, y, 2, 2, 0xFF12401F)
        frames.append(g)
    return frames


def progress_frames():
    base = parse(ARROW)
    frames = []
    for f in range(4):
        g = [r[:] for r in base]
        for i in range(4):
            col = COL['G'] if i == f else 0xFF12401F
            g = dot(g, 10 + (i % 2) * 3, 10 + (i // 2) * 3, 2, 2, col)
        frames.append(g)
    return frames


def build_shapes():
    arrow = parse(ARROW)
    hd = parse(HDOUBLE)
    vd = transpose(hd)
    d1 = parse(DIAG1)          # nwse
    d2 = flip_h(d1)            # nesw
    text = parse(TEXT)
    zoom = parse(ZOOM)

    shapes = {}

    def add(names, grids, hot, delay=0):
        if not isinstance(grids[0][0], list):
            grids = [grids]        # single grid -> one frame
        shapes[names[0]] = (names, grids, hot, delay)

    add(["default", "left_ptr", "arrow", "top_left_arrow"], arrow, (1, 1))
    add(["pointer", "hand", "hand1", "hand2", "pointing_hand"], parse(POINTER), (7, 1))
    add(["text", "xterm", "ibeam"], text, (4, 8))
    add(["vertical-text"], transpose(text), (8, 4))
    add(["crosshair", "cross", "tcross"], parse(CROSSHAIR), (7, 7))
    add(["move", "fleur", "size_all", "all-scroll"], parse(MOVE), (7, 7))
    add(["grab", "openhand"], parse(GRAB), (7, 7))
    add(["grabbing", "closedhand", "dnd-move", "dnd-none"], parse(GRABBING), (7, 8))
    add(["not-allowed", "crossed_circle", "forbidden", "no-drop", "dnd-no-drop"],
        parse(NOTALLOWED), (7, 7))
    add(["wait", "watch"], wait_frames(), (7, 7), delay=110)
    add(["progress", "left_ptr_watch", "half-busy"], progress_frames(), (1, 1),
        delay=160)

    add(["ew-resize", "h_double_arrow", "sb_h_double_arrow", "size_hor",
         "col-resize", "split_h", "w-resize", "left_side",
         "e-resize", "right_side"], hd, (7, 8))
    add(["ns-resize", "v_double_arrow", "sb_v_double_arrow", "size_ver",
         "row-resize", "split_v", "n-resize", "top_side",
         "s-resize", "bottom_side"], vd, (8, 7))
    add(["nwse-resize", "size_fdiag", "bd_double_arrow",
         "nw-resize", "top_left_corner",
         "se-resize", "bottom_right_corner"], d1, (7, 7))
    add(["nesw-resize", "size_bdiag", "fd_double_arrow",
         "ne-resize", "top_right_corner",
         "sw-resize", "bottom_left_corner"], d2, (8, 7))

    add(["help", "question_arrow", "left_ptr_help", "whats_this"],
        stamp(arrow, OV_QUESTION, 9, 7), (1, 1))
    add(["copy", "dnd-copy"], stamp(arrow, OV_PLUS, 10, 9), (1, 1))
    add(["alias", "dnd-link", "dnd-ask"], stamp(arrow, OV_LINK, 9, 9), (1, 1))
    add(["context-menu"], stamp(arrow, OV_MENU, 9, 9), (1, 1))
    add(["cell", "plus"], parse(CELL), (7, 7))
    add(["zoom-in", "zoom_in"], stamp(zoom, OV_PLUS, 3, 2), (5, 5))
    add(["zoom-out", "zoom_out"],
        stamp(dot(zoom, 3, 4, 5, 1, COL['G']), OV_PLUS, 20, 20), (5, 5))
    return shapes


# ------------------------------------------------------------ xcursor file

def rim(g):
    # One-cell pale halo around the OUTER silhouette: keeps the dark outline
    # visible on dark surfaces. Interior holes (the ? counter, the lens) are
    # left alone — only transparency reachable from the grid edge gets rimmed.
    n = len(g)
    outside = [[False] * n for _ in range(n)]
    stack = [(x, y) for x in range(n) for y in (0, n - 1) if not g[y][x]] + \
            [(x, y) for y in range(n) for x in (0, n - 1) if not g[y][x]]
    while stack:
        x, y = stack.pop()
        if not (0 <= x < n and 0 <= y < n) or outside[y][x] or g[y][x]:
            continue
        outside[y][x] = True
        stack += [(x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)]
    out = [r[:] for r in g]
    for y in range(n):
        for x in range(n):
            if not outside[y][x]:
                continue
            if any(g[y + dy][x + dx]
                   for dy in (-1, 0, 1) for dx in (-1, 0, 1)
                   if 0 <= y + dy < n and 0 <= x + dx < n):
                out[y][x] = RIM
    return out


def scale2x(g):
    # EPX/Scale2x: doubles pixel art while rounding staircase edges. Keeps
    # the retro look but reads as crisp, not blocky, at 64/96px.
    n = len(g)
    out = [[0] * (n * 2) for _ in range(n * 2)]
    for y in range(n):
        for x in range(n):
            p = g[y][x]
            a = g[y - 1][x] if y > 0 else 0
            b = g[y][x + 1] if x < n - 1 else 0
            c = g[y][x - 1] if x > 0 else 0
            d = g[y + 1][x] if y < n - 1 else 0
            e1 = a if (c == a and c != d and a != b) else p
            e2 = b if (a == b and a != c and b != d) else p
            e3 = c if (d == c and d != b and c != a) else p
            e4 = d if (b == d and b != a and d != c) else p
            out[2 * y][2 * x] = e1
            out[2 * y][2 * x + 1] = e2
            out[2 * y + 1][2 * x] = e3
            out[2 * y + 1][2 * x + 1] = e4
    return out


def scale_grid(g, size):
    # Upscale through the scale2x chain to at least the target, then
    # nearest-sample down to the exact size.
    src = g
    while len(src) < size:
        src = scale2x(src)
    n = len(src)
    px = [[0] * size for _ in range(size)]
    for y in range(size):
        for x in range(size):
            px[y][x] = src[y * n // size][x * n // size]
    return px


def write_xcursor(path, grids, hot, delay):
    # One image chunk per (size, frame); frames with equal nominal size form
    # the animation sequence, played with `delay` ms per frame.
    chunks = []
    for size in SIZES:
        for g in grids:
            px = scale_grid(rim(g), size)
            hx = hot[0] * size // GRID
            hy = hot[1] * size // GRID
            pixels = b''.join(struct.pack('<I', p) for row in px for p in row)
            head = struct.pack('<9I', 36, 0xFFFD0002, size, 1,
                               size, size, hx, hy, delay)
            chunks.append((size, head + pixels))

    ntoc = len(chunks)
    pos = 16 + ntoc * 12
    toc = b''
    body = b''
    for size, data in chunks:
        toc += struct.pack('<3I', 0xFFFD0002, size, pos)
        body += data
        pos += len(data)

    with open(path, 'wb') as f:
        f.write(b'Xcur' + struct.pack('<3I', 16, 0x10000, ntoc))
        f.write(toc)
        f.write(body)


def preview(shapes, out):
    # Contact sheet of every primary shape at 32px, PPM.
    names = sorted(shapes)
    cols = 8
    rows = (len(names) + cols - 1) // cols
    W, H = cols * 40, rows * 40
    buf = [[0x000A03] * W for _ in range(H)]
    for i, n in enumerate(names):
        g = scale_grid(rim(shapes[n][1][0]), 32)
        ox, oy = (i % cols) * 40 + 4, (i // cols) * 40 + 4
        for y in range(32):
            for x in range(32):
                p = g[y][x]
                if p >> 24:
                    buf[oy + y][ox + x] = p & 0xFFFFFF
    with open(out, 'wb') as f:
        f.write(b'P6\n%d %d\n255\n' % (W, H))
        for row in buf:
            f.write(bytes(b for p in row
                          for b in ((p >> 16) & 255, (p >> 8) & 255, p & 255)))


def main():
    outdir = sys.argv[1]
    curdir = os.path.join(outdir, 'cursors')
    os.makedirs(curdir, exist_ok=True)

    shapes = build_shapes()
    for primary, (names, grids, hot, delay) in shapes.items():
        write_xcursor(os.path.join(curdir, primary), grids, hot, delay)
        for alias in names[1:]:
            link = os.path.join(curdir, alias)
            if os.path.lexists(link):
                os.unlink(link)
            os.symlink(primary, link)

    with open(os.path.join(outdir, 'index.theme'), 'w') as f:
        f.write("[Icon Theme]\n"
                "Name=KDOS-cursors\n"
                "Comment=KDOS phosphor pixel cursors\n"
                "Inherits=\n")

    if '--preview' in sys.argv:
        preview(shapes, os.path.join(outdir, 'preview.ppm'))
    print("kdos-cursors: %d shapes (+aliases) -> %s" % (len(shapes), curdir))


if __name__ == '__main__':
    main()
