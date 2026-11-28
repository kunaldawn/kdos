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
# Draw the artwork of kdos.dev into site/assets/art/, and the web fonts the
# page sets its type in into site/assets/fonts/.
#
# MAINTENANCE TOOL, run by hand; its output is committed and the site build
# only copies it. It reads everything it draws from the tree, so a change to
# any input is a re-run, never a hand edit of a PNG:
#
#   src/libs/libkcolor/kcolor.h      the eight accents (the KCOL_SCHEMES
#                                    X-macro) and the default one
#   fs/usr/share/kdos/logo.txt       the block wordmark
#   src/art/kdos-splash/penguin.h    the boot splash's penguin
#   kdos.png                         the master artwork, for the large icons
#   src/art/kdos-icons/marks/        the penguin marks, for the small icons
#   build/fs/usr/share/fonts/        Terminus and misc-fixed bitmaps, and the
#                                    TTFs the web fonts are made from
#
# THE COLOURS ARE READ, NEVER WRITTEN HERE. A hex literal for an accent in
# this file would be a second copy of the palette, and the copy is the one
# that goes stale. The only literals are colours that belong to no scheme:
# the monitor's plastic and its power lamp, and the penguin's black.
#
# THE TEXT IS BITMAP TEXT. Every caption is blitted from the PCF strikes of
# Terminus and misc-fixed, glyph by glyph, then scaled by whole numbers with
# nearest-neighbour: an outline font rasterised at these sizes is grey mush,
# and resampling by a fraction blurs the pixel grid the look depends on.
#
# DETERMINISTIC: no clock, no randomness, no metadata in the PNGs. The same
# inputs and the same Pillow give the same pixels; --check proves it against
# what is committed.
#
#   python3 site/art/genart.py              draw every image, art.tsv and
#                                           inputs.tsv
#   python3 site/art/genart.py --check      redraw in memory; exit 1 on drift
#   python3 site/art/genart.py --fonts      also write site/assets/fonts/
#   ... --fontdir DIR                       fonts from DIR, not build/fs
#
# Any Python 3 with Pillow draws the art. --fonts needs fontTools and brotli
# as well, which the built tree's own interpreter carries; from the root of
# the checkout:
#
#   PYTHONHOME=$PWD/build/fs/usr build/fs/usr/lib/libc.so \
#       --library-path build/fs/usr/lib build/fs/usr/bin/python3 -I \
#       site/art/genart.py --fonts

import argparse
import gzip
import hashlib
import io
import os
import re
import sys

from PIL import Image, ImageChops, ImageDraw, ImageFilter, PcfFontFile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..', '..'))
OUT = os.path.join(ROOT, 'site', 'assets', 'art')
FONTS_OUT = os.path.join(ROOT, 'site', 'assets', 'fonts')

KCOLOR = os.path.join(ROOT, 'src', 'libs', 'libkcolor', 'kcolor.h')
LOGO = os.path.join(ROOT, 'fs', 'usr', 'share', 'kdos', 'logo.txt')
PENGUIN = os.path.join(ROOT, 'src', 'art', 'kdos-splash', 'penguin.h')
MASTER = os.path.join(ROOT, 'kdos.png')
MARKS = os.path.join(ROOT, 'src', 'art', 'kdos-icons', 'marks')
FONTDIR = os.path.join(ROOT, 'build', 'fs', 'usr', 'share', 'fonts')

# What the artwork says. The site's own copy says the same; these are the
# lines to edit when it changes.
NAME = 'Kool Desktop Operating System'
BLURB = 'A Linux distribution compiled from source'
STACK = 'musl · GNU · sysvinit · KDE Plasma'
SITE = 'kdos.dev'

WHITE = (255, 255, 255)
BLACK = (0, 0, 0)


# ── colour ───────────────────────────────────────────────────────────────

def rgb(h):
    return (int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16))


def mix(a, b, t):
    """a moved a fraction t of the way to b."""
    return tuple(int(round(x + (y - x) * t)) for x, y in zip(a, b))


def lum(c):
    def ch(v):
        v /= 255.0
        return v / 12.92 if v <= 0.03928 else ((v + 0.055) / 1.055) ** 2.4
    r, g, b = (ch(v) for v in c[:3])
    return 0.2126 * r + 0.7152 * g + 0.0722 * b


def contrast(a, b):
    la, lb = sorted((lum(a), lum(b)), reverse=True)
    return (la + 0.05) / (lb + 0.05)


def is_light(sc):
    return lum(sc['deep']) > 0.5


# ── inputs ───────────────────────────────────────────────────────────────

KCOL_FIELDS = ('primary', 'dim', 'secondary', 'urgent', 'deep', 'text',
               'variant', 'pdark', 'backdrop')
KCOL_ROW = re.compile(r'X\(\s*(\w+)\s*,\s*"([^"]*)"\s*,\s*"([^"]*)"\s*,\s*([^)]*)\)')


def read_schemes(path):
    """libkcolor's KCOL_SCHEMES rows in table order, and the default's name."""
    with open(path, encoding='utf-8') as f:
        text = f.read()
    m = re.search(r'#define\s+KCOL_SCHEMES\(X\)(.*?)(?:\n\s*\n|\n#)', text, re.S)
    if not m:
        raise SystemExit('genart: %s: no KCOL_SCHEMES table' % path)
    body = m.group(1).replace('\\\n', ' ')
    out = []
    for name, label, theme, vals in KCOL_ROW.findall(body):
        hexes = [v.strip() for v in vals.split(',')]
        if len(hexes) != len(KCOL_FIELDS):
            raise SystemExit('genart: %s: %s has %d colours' % (path, name, len(hexes)))
        sc = dict(zip(KCOL_FIELDS, (rgb(h) for h in hexes)))
        sc.update(name=name, label=label, theme=theme)
        out.append(sc)
    d = re.search(r'#define\s+KCOL_DEFAULT_ID\s+(\w+)', text)
    if not out or not d or d.group(1) not in [s['name'] for s in out]:
        raise SystemExit('genart: %s: no schemes or no default' % path)
    return out, d.group(1)


SGR = re.compile(r'\x1b\[([0-9;]*)m')


def ansi_cells(line):
    """[(char, sgr)] for one line of logo.txt; sgr is the last code set."""
    cells, cur, i = [], '0', 0
    while i < len(line):
        m = SGR.match(line, i)
        if m:
            cur = m.group(1) or '0'
            i = m.end()
            continue
        cells.append((line[i], cur))
        i += 1
    return cells


def read_wordmark(path):
    """The block wordmark of the fastfetch logo: the rows before its first
    blank line, each a list of (char, sgr) cells."""
    with open(path, encoding='utf-8') as f:
        lines = f.read().split('\n')
    blank = next(i for i, l in enumerate(lines) if not SGR.sub('', l).strip())
    if blank == 0:
        raise SystemExit('genart: %s: no wordmark before the first blank line' % path)
    rows = [ansi_cells(l) for l in lines[:blank]]
    # draw_cell knows these and nothing else; any other glyph would vanish
    # from the picture without a word
    odd = {c for row in rows for c, _ in row} - set(' █═║╔╗╚╝')
    if odd:
        raise SystemExit('genart: %s: the wordmark uses %s, which draw_cell does not draw'
                         % (path, ' '.join(sorted(odd))))
    return rows


def read_penguin(path):
    """penguin.h as a mode-L image of palette indices (0 is transparent)."""
    with open(path, encoding='utf-8') as f:
        src = f.read()
    w = int(re.search(r'#define\s+PENGUIN_W\s+(\d+)', src).group(1))
    h = int(re.search(r'#define\s+PENGUIN_H\s+(\d+)', src).group(1))
    body = src[src.index('penguin_rle[]'):]
    body = body[body.index('{') + 1:body.index('}')]
    nums = [int(n) for n in re.findall(r'\d+', body)]
    data = bytearray()
    for i in range(0, len(nums) - 1, 2):
        data += bytes((nums[i],)) * nums[i + 1]
    if len(data) != w * h:
        raise SystemExit('genart: %s: %d pixels, want %d' % (path, len(data), w * h))
    return Image.frombytes('L', (w, h), bytes(data))


def penguin_palette(sc):
    """The splash's mapping of penguin.h's indices onto a scheme.

    Index 1 is the bird's own black and stays black under every accent; the
    belly is the scheme's text pushed toward white, the beak and feet its
    secondary, the rim its primary and pdark. Same mapping kdos-splash makes
    at boot, so the page's penguin and the boot screen's are one bird.
    """
    return {1: BLACK, 2: mix(sc['text'], WHITE, 0.68), 3: sc['secondary'],
            4: sc['primary'], 5: sc['pdark']}


# ── bitmap fonts ─────────────────────────────────────────────────────────

class BitFont:
    """A PCF strike, drawn glyph by glyph. Latin-1 only, which is all the
    artwork writes; box drawing is drawn by draw_cell, not by a font."""

    def __init__(self, path):
        with gzip.open(path) if path.endswith('.gz') else open(path, 'rb') as f:
            pcf = PcfFontFile.PcfFontFile(io.BytesIO(f.read()), 'iso8859-1')
        self.glyph = pcf.glyph
        boxes = [g[1] for g in self.glyph if g]
        self.ascent = max(-b[1] for b in boxes)
        self.descent = max(b[3] for b in boxes)
        self.height = self.ascent + self.descent

    def width(self, text):
        return sum(self.glyph[ord(c)][0][0] for c in text if self.glyph[ord(c)])

    def draw(self, im, xy, text, fill):
        """Draw with the TOP of the line box at xy[1]."""
        x, y = xy
        base = y + self.ascent
        for c in text:
            g = self.glyph[ord(c)]
            if not g:
                continue
            adv, dst, src, bm = g
            if bm is not None and dst[2] > dst[0] and dst[3] > dst[1]:
                im.paste(fill, (x + dst[0], base + dst[1]), bm.crop(src))
            x += adv[0]
        return x


FONT_FILES = {
    't12': 'terminus/ter-x12n.pcf.gz', 't12b': 'terminus/ter-x12b.pcf.gz',
    't14b': 'terminus/ter-x14b.pcf.gz', 't16b': 'terminus/ter-x16b.pcf.gz',
    't24b': 'terminus/ter-x24b.pcf.gz', 't32b': 'terminus/ter-x32b.pcf.gz',
    'f4x6': 'misc/4x6.pcf.gz', 'f5x7': 'misc/5x7.pcf.gz', 'f6x10': 'misc/6x10.pcf.gz',
}


def load_fonts(fontdir):
    fonts = {}
    for key, rel in FONT_FILES.items():
        p = os.path.join(fontdir, rel)
        if not os.path.exists(p):
            raise SystemExit('genart: %s is missing; build the tree or pass --fontdir' % p)
        fonts[key] = BitFont(p)
    return fonts


# ── drawing primitives ───────────────────────────────────────────────────

def load_rgba(path):
    with Image.open(path) as f:
        return f.convert('RGBA')


def nearest(im, k):
    return im if k == 1 else im.resize((im.width * k, im.height * k), Image.NEAREST)


def rect(d, x, y, w, h, fill):
    if w > 0 and h > 0:
        d.rectangle((x, y, x + w - 1, y + h - 1), fill=fill)


def draw_cell(d, x, y, cw, ch, c, fill, t=1, gap=1):
    """One character cell of a block banner, as a terminal font draws it.

    A full block fills the cell edge to edge so neighbours fuse into one bar;
    the double-line box characters are two strokes of t pixels with gap
    between, centred, and their corners join outer-to-outer and
    inner-to-inner the way the glyphs do.
    """
    if c == '█':
        rect(d, x, y, cw, ch, fill)
        return
    span = 2 * t + gap
    a1 = x + (cw - span) // 2          # left vertical stroke
    a2 = a1 + t + gap                  # right vertical stroke
    b1 = y + (ch - span) // 2          # upper horizontal stroke
    b2 = b1 + t + gap                  # lower horizontal stroke
    r, bm = x + cw, y + ch
    if c == '═':
        rect(d, x, b1, cw, t, fill); rect(d, x, b2, cw, t, fill)
    elif c == '║':
        rect(d, a1, y, t, ch, fill); rect(d, a2, y, t, ch, fill)
    elif c == '╔':
        rect(d, a1, b1, r - a1, t, fill); rect(d, a1, b1, t, bm - b1, fill)
        rect(d, a2, b2, r - a2, t, fill); rect(d, a2, b2, t, bm - b2, fill)
    elif c == '╗':
        rect(d, x, b1, a2 + t - x, t, fill); rect(d, a2, b1, t, bm - b1, fill)
        rect(d, x, b2, a1 + t - x, t, fill); rect(d, a1, b2, t, bm - b2, fill)
    elif c == '╚':
        rect(d, a1, y, t, b2 + t - y, fill); rect(d, a1, b2, r - a1, t, fill)
        rect(d, a2, y, t, b1 + t - y, fill); rect(d, a2, b1, r - a2, t, fill)
    elif c == '╝':
        rect(d, a2, y, t, b2 + t - y, fill); rect(d, x, b2, a2 + t - x, t, fill)
        rect(d, a1, y, t, b1 + t - y, fill); rect(d, x, b1, a1 + t - x, t, fill)


def glow(layer, colour, radii):
    """layer over a halo of colour spread from its alpha: one Gaussian per
    (radius, gain), summed, so a tight bright core sits in a wide dim bloom."""
    a = layer.getchannel('A')
    acc = Image.new('L', layer.size, 0)
    for r, k in radii:
        b = a.filter(ImageFilter.GaussianBlur(r))
        acc = ImageChops.add(acc, b.point(lambda v, k=k: min(255, int(v * k))))
    halo = Image.new('RGBA', layer.size, colour + (0,))
    halo.putalpha(acc)
    halo.alpha_composite(layer)
    return halo


def scanlines(im, period=3, alpha=60, rows=1):
    """Darken `rows` of every `period` rows, as a CRT's unlit gaps do."""
    sl = Image.new('RGBA', im.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(sl)
    for y in range(period - rows, im.height, period):
        d.rectangle((0, y, im.width - 1, y + rows - 1), fill=(0, 0, 0, alpha))
    out = im.copy()
    out.alpha_composite(sl)
    return out


def vignette(size, colour, inner, alpha):
    """A radial falloff to colour at the corners, as a tube's edge darkens."""
    w, h = size
    small = Image.new('L', (64, 64), 0)
    px = small.load()
    for y in range(64):
        for x in range(64):
            dx, dy = (x - 31.5) / 31.5, (y - 31.5) / 31.5
            r = (dx * dx + dy * dy) ** 0.5 / 1.4142
            v = 0.0 if r < inner else min(1.0, (r - inner) / (1 - inner))
            px[x, y] = int(alpha * v * v)
    mask = small.resize(size, Image.BICUBIC)
    lay = Image.new('RGBA', size, colour + (0,))
    lay.putalpha(mask)
    return lay


# ── the wordmark ─────────────────────────────────────────────────────────

def wordmark(rows, sc, cw=8, ch=16, t=1, gap=2, hot=0.18):
    """The block wordmark on a transparent ground, one pixel per pixel.

    logo.txt writes the top half bold and the bottom half plain: bright
    above, darker below. Drawn as a picture, that step is a ramp, row by row,
    from the hot core of a phosphor (the accent pushed toward white) down to
    the accent part-way to pdark. The box-drawing strokes are the letters'
    drop shadow and take the dark end, so the blocks stand off them.
    """
    cols = max(len(r) for r in rows)
    im = Image.new('RGBA', (cols * cw, len(rows) * ch), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    light = is_light(sc)
    top = mix(sc['primary'], WHITE, 0 if light else hot)
    low = mix(sc['primary'], sc['pdark'], 0.2 if light else 0.4)
    edge = mix(sc['primary'], sc['deep'] if light else sc['pdark'], 0.45 if light else 0.6)
    n = max(1, len(rows) - 1)
    for j, row in enumerate(rows):
        block = mix(top, low, j / n)
        for i, (c, _) in enumerate(row):
            col = block if c == '█' else edge
            draw_cell(d, i * cw, j * ch, cw, ch, c, col + (255,), t, gap)
    return im


def banner(rows, sc, scale=3, pad=10):
    art = wordmark(rows, sc)
    canvas = Image.new('RGBA', (art.width + 2 * pad, art.height + 2 * pad), (0, 0, 0, 0))
    canvas.alpha_composite(art, (pad, pad))
    canvas = nearest(canvas, scale)
    if is_light(sc):
        return glow(canvas, sc['primary'], ((3, 0.35), (10, 0.2)))
    return glow(canvas, sc['primary'], ((2, 0.9), (7, 0.55), (18, 0.4)))


# ── the penguins ─────────────────────────────────────────────────────────

def splash_penguin(idx, sc, scale):
    pal = penguin_palette(sc)
    im = Image.new('RGBA', idx.size, (0, 0, 0, 0))
    src = idx.load()
    dst = im.load()
    for y in range(idx.height):
        for x in range(idx.width):
            v = src[x, y]
            if v:
                dst[x, y] = pal[v] + (255,)
    return nearest(im, scale)


# ── pixel icons ──────────────────────────────────────────────────────────
#
# Hand-set at 16x16. '#' is ink, '+' a half-tone, '-' a faint tone, '.'
# clear. They are shipped white on transparent so the page can paint them in
# the live accent through CSS mask-image, or show them as they are on a dark
# ground.

ICONS = {
    'floppy': (
        'a 3.5-inch floppy disk', 'saving or downloading',
        ['###############.',
         '#++#########+#+#',
         '#++#....#+#+#++#',
         '#++#....#+#+#++#',
         '#++#....###+#++#',
         '#++#########++++',
         '#++++++++++++++#',
         '#+############+#',
         '#+#..........#+#',
         '#+#.########.#+#',
         '#+#..........#+#',
         '#+#.######...#+#',
         '#+#..........#+#',
         '#+#..........#+#',
         '#+############+#',
         '################']),
    'book': (
        'an open book', 'the documentation',
        ['................',
         '.######..######.',
         '#++++++##++++++#',
         '#+####+##+####+#',
         '#++++++##++++++#',
         '#+####+##+####+#',
         '#++++++##++++++#',
         '#+###++##+###++#',
         '#++++++##++++++#',
         '#+####+##+####+#',
         '#++++++##++++++#',
         '#++++++##++++++#',
         '#######..#######',
         '.......##.......',
         '................',
         '................']),
    'terminal': (
        'a terminal window with a prompt', 'the command line',
        ['################',
         '#++++++++++#+#+#',
         '################',
         '#..............#',
         '#.#............#',
         '#..#...........#',
         '#...#..........#',
         '#..#...........#',
         '#.#..#####.....#',
         '#..............#',
         '#..............#',
         '################',
         '......#..#......',
         '....########....',
         '...##########...',
         '................']),
    'package': (
        'a taped cardboard box', 'packages',
        ['................',
         '....###########.',
         '...#---##----##.',
         '..#---##----#+#.',
         '.############++#',
         '.#+++##+++++#++#',
         '.#+++##+++++#++#',
         '.#+++##+++++#++#',
         '.#+++++++++++#+#',
         '.#+++++++++++#+#',
         '.#+###+++++++#+#',
         '.#+++++++++++##.',
         '.#+##++++++++#..',
         '.#############..',
         '................',
         '................']),
    'slots': (
        'two slots, A and B, with an arrow between them', 'A/B updates',
        ['................',
         '#######..#######',
         '#+++++#..#+++++#',
         '#++#++#..#+##++#',
         '#+#+#+#..#+#+#+#',
         '#+###+#..#+##++#',
         '#+#+#+#..#+#+#+#',
         '#+#+#+#..#+##++#',
         '#+++++#..#+++++#',
         '#######..#######',
         '................',
         '...#........#...',
         '..############..',
         '...#........#...',
         '................',
         '................']),
    'window': (
        'a desktop window with a title bar', 'the desktop',
        ['################',
         '#++++++++#.#.#.#',
         '################',
         '#......#.......#',
         '#.####.#.#####.#',
         '#......#.......#',
         '#.###..#.###...#',
         '#......#.......#',
         '#.####.#.####..#',
         '#......#.......#',
         '#.##...#.#####.#',
         '#......#.......#',
         '################',
         '................',
         '................',
         '................']),
    'shield': (
        'a shield with a keyhole', 'security',
        ['.......##.......',
         '....###++###....',
         '.###++++++++###.',
         '.#++++++++++++#.',
         '.#+++++##+++++#.',
         '.#++++#..#++++#.',
         '.#++++#..#++++#.',
         '.#+++++..+++++#.',
         '.#+++++..+++++#.',
         '.#++++#..#++++#.',
         '..#+++####+++#..',
         '..#++++++++++#..',
         '...#++++++++#...',
         '....##++++##....',
         '......####......',
         '................']),
    'code': (
        'angle brackets round a slash, as in source code', 'building from source',
        ['................',
         '..........#.....',
         '..........#.....',
         '.........#......',
         '...##....#..##..',
         '..##....#....##.',
         '.##.....#.....##',
         '##.....#.......#',
         '##.....#.......#',
         '.##...#.......##',
         '..##..#......##.',
         '...##.#.....##..',
         '.....#..........',
         '.....#..........',
         '................',
         '................']),
}

TONES = {'#': 255, '+': 120, '-': 50}


def icon(rows):
    im = Image.new('RGBA', (16, 16), (255, 255, 255, 0))
    px = im.load()
    for y, row in enumerate(rows):
        for x, c in enumerate(row[:16]):
            if c in TONES:
                px[x, y] = (255, 255, 255, TONES[c])
    return im


# ── PNG output ───────────────────────────────────────────────────────────

def as_palette(im):
    """im as an exact palette image when it has 256 colours or fewer.

    Flat pixel art in RGBA is four bytes a pixel for a handful of colours;
    the palette form of the same pixels is a fraction of the file. Exact,
    never quantised: a colour that moved would be an accent that moved.
    """
    rgba = im.convert('RGBA')
    colours = rgba.getcolors(256)
    if colours is None:
        return None
    colours.sort(key=lambda c: (c[1][3], c[1]))
    index = {bytes(c): i for i, (_, c) in enumerate(colours)}
    raw = rgba.tobytes()
    pal = Image.frombytes('P', rgba.size,
                          bytes(index[raw[i:i + 4]] for i in range(0, len(raw), 4)))
    flat = []
    for _, c in colours:
        flat += c[:3]
    pal.putpalette(flat)
    alphas = bytes(c[3] for _, c in colours)
    if any(a != 255 for a in alphas):
        pal.info['transparency'] = alphas
    return pal


def png_bytes(im):
    pal = as_palette(im)
    buf = io.BytesIO()
    if pal is not None:
        kw = {'transparency': pal.info['transparency']} if 'transparency' in pal.info else {}
        pal.save(buf, 'PNG', optimize=True, **kw)
    else:
        im.save(buf, 'PNG', optimize=True)
    return buf.getvalue()


# ── the pieces ───────────────────────────────────────────────────────────

class Art:
    """Collects (file, bytes-or-image, purpose, alt) in drawing order."""

    def __init__(self):
        self.items = []

    def add(self, name, im, purpose, alt):
        self.items.append((name, im, purpose, alt))


def make_banners(art, schemes, words):
    for sc in schemes:
        art.add('banner-%s.png' % sc['name'], banner(words, sc),
                'the KDOS block wordmark with a phosphor glow in the %s accent, '
                'transparent; the hero swaps it with the accent' % sc['label'],
                'KDOS')


def make_penguins(art, schemes, idx):
    for sc in schemes:
        art.add('penguin-%s.png' % sc['name'], splash_penguin(idx, sc, 3),
                'the boot splash penguin, 182x192 at 3x nearest-neighbour, its '
                'rim, belly and beak in the %s accent as the splash draws them, '
                'transparent' % sc['label'],
                'The KDOS penguin, a muscular Tux flexing both arms')


def make_bezel(art, fonts):
    """A beige CRT monitor face with the screen cut out.

    Drawn at twice the size and reduced, so its curves are smooth: it is an
    object in the picture, not pixel art. The screen aperture is 16:9 to take
    the rig's 1920x1080 shots, its rectangle is in art.tsv, and the darkening
    at its edges and the faint glare are drawn over the aperture at low alpha
    so they sit on top of whatever screenshot the page lays under it.
    """
    S = 2
    W, H = 1280, 824
    SX, SY, SW, SH = 96, 72, 1088, 612
    w, h = W * S, H * S
    sx, sy, sw, sh = SX * S, SY * S, SW * S, SH * S

    plastic_hi, plastic, plastic_lo = (230, 222, 199), (214, 205, 180), (176, 166, 140)
    shell = Image.new('RGBA', (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(shell)
    # the case: a dark rim, a light bevel, the face
    d.rounded_rectangle((0, 0, w - 1, h - 1), 40 * S, fill=plastic_lo + (255,))
    d.rounded_rectangle((0, 0, w - 1 - 3 * S, h - 1 - 4 * S), 38 * S, fill=plastic_hi + (255,))
    d.rounded_rectangle((3 * S, 3 * S, w - 1 - 4 * S, h - 1 - 6 * S), 36 * S, fill=plastic + (255,))
    # a soft vertical sheen on the face
    sheen = Image.new('L', (1, 64))
    for y in range(64):
        sheen.putpixel((0, y), int(36 * (1 - y / 63.0)))
    sheen = sheen.resize((w, h), Image.BICUBIC)
    face = Image.new('L', (w, h), 0)
    ImageDraw.Draw(face).rounded_rectangle((3 * S, 3 * S, w - 1 - 4 * S, h - 1 - 6 * S), 36 * S, fill=255)
    lift = Image.new('RGBA', (w, h), WHITE + (0,))
    lift.putalpha(ImageChops.multiply(sheen, face))
    shell.alpha_composite(lift)

    # the recess the tube sits in: shadowed along its top and left edges
    m = 34 * S
    rx0, ry0, rx1, ry1 = sx - m, sy - m, sx + sw + m, sy + sh + m
    d.rounded_rectangle((rx0, ry0, rx1, ry1), 26 * S, fill=(190, 181, 155, 255))
    d.rounded_rectangle((rx0 + 2 * S, ry0 + 2 * S, rx1, ry1), 25 * S, fill=(205, 196, 170, 255))
    d.rounded_rectangle((rx0 + 3 * S, ry0 + 3 * S, rx1 - 1 * S, ry1 - 1 * S), 24 * S, fill=(200, 191, 165, 255))
    lip = Image.new('L', (w, h), 0)
    ImageDraw.Draw(lip).rounded_rectangle((rx0, ry0, rx1, ry1), 26 * S, fill=255)
    inner = lip.filter(ImageFilter.GaussianBlur(10 * S))
    shadow = ImageChops.subtract(lip, inner).point(lambda v: min(255, v * 2))
    sh_l = Image.new('RGBA', (w, h), (60, 50, 30, 0))
    sh_l.putalpha(shadow.point(lambda v: v // 3))
    shell.alpha_composite(sh_l)

    # the black glass border round the picture
    g = 16 * S
    d.rounded_rectangle((sx - g, sy - g, sx + sw + g, sy + sh + g), 30 * S, fill=(22, 22, 20, 255))
    d.rounded_rectangle((sx - g + 2 * S, sy - g + 2 * S, sx + sw + g - 2 * S, sy + sh + g - 2 * S),
                        28 * S, fill=(12, 12, 11, 255))

    # the picture itself is a hole
    hole = Image.new('L', (w, h), 0)
    ImageDraw.Draw(hole).rounded_rectangle((sx, sy, sx + sw - 1, sy + sh - 1), 18 * S, fill=255)
    shell.putalpha(ImageChops.subtract(shell.getchannel('A'), hole))

    # the tube's edge darkens, and the glass catches a little light
    edge = ImageChops.subtract(hole, hole.filter(ImageFilter.GaussianBlur(12 * S)))
    edge = edge.point(lambda v: min(120, v * 2))
    dark = Image.new('RGBA', (w, h), (0, 0, 0, 0))
    dark.putalpha(edge)
    shell.alpha_composite(dark)
    glare = Image.new('L', (w, h), 0)
    ImageDraw.Draw(glare).ellipse((sx - sw // 6, sy - sh // 2, sx + sw // 2, sy + sh // 3), fill=22)
    glare = ImageChops.multiply(glare.filter(ImageFilter.GaussianBlur(60 * S)).point(lambda v: min(255, v * 2)), hole)
    gl = Image.new('RGBA', (w, h), WHITE + (0,))
    gl.putalpha(glare)
    shell.alpha_composite(gl)

    shell = shell.resize((W, H), Image.LANCZOS)

    # the chin: badge, vents, power switch and its lamp, at 1x on the grid
    d = ImageDraw.Draw(shell)
    chin_y = SY + SH + 34 + 22
    label = Image.new('RGBA', (80, 20), (0, 0, 0, 0))
    fonts['t16b'].draw(label, (0, 2), 'KDOS', (120, 112, 90, 255))
    fonts['t16b'].draw(label, (0, 1), 'KDOS', (160, 151, 126, 255))
    label = nearest(label.crop(label.getbbox()), 2)
    shell.alpha_composite(label, (SX + 4, chin_y + 8))
    for i in range(9):
        y = chin_y + 4 + i * 6
        d.rounded_rectangle((W // 2 - 150, y, W // 2 + 150, y + 2), 1, fill=(168, 158, 132, 255))
        d.line((W // 2 - 149, y + 3, W // 2 + 149, y + 3), fill=(226, 218, 195, 255))
    for k in range(2):
        kx, ky = SX + SW - 230 + k * 44, chin_y + 24
        d.ellipse((kx - 14, ky - 14, kx + 14, ky + 14), fill=(150, 141, 116, 255))
        d.ellipse((kx - 13, ky - 13, kx + 12, ky + 12), fill=(232, 225, 204, 255))
        d.ellipse((kx - 11, ky - 11, kx + 12, ky + 12), fill=(200, 191, 165, 255))
        d.ellipse((kx - 10, ky - 10, kx + 10, ky + 10), fill=(214, 205, 180, 255))
        d.line((kx, ky - 9, kx, ky - 3), fill=(120, 112, 90, 255), width=2)
    bx = SX + SW - 92
    d.rounded_rectangle((bx - 1, chin_y + 7, bx + 57, chin_y + 41), 7, fill=(150, 141, 116, 255))
    d.rounded_rectangle((bx, chin_y + 8, bx + 56, chin_y + 40), 6, fill=(222, 214, 190, 255))
    d.rounded_rectangle((bx + 2, chin_y + 10, bx + 54, chin_y + 38), 5, fill=(208, 199, 173, 255))
    lamp = Image.new('RGBA', (60, 60), (0, 0, 0, 0))
    ImageDraw.Draw(lamp).ellipse((24, 24, 35, 35), fill=(120, 255, 90, 255))
    lamp = glow(lamp, (80, 255, 60), ((3, 0.8), (8, 0.4)))
    ImageDraw.Draw(lamp).ellipse((27, 27, 31, 31), fill=(220, 255, 210, 255))
    shell.alpha_composite(lamp, (bx - 52, chin_y - 6))

    art.add('crt-bezel.png', shell,
            'a beige CRT monitor face with a transparent 16:9 screen at '
            'x=%d y=%d w=%d h=%d (in %% of the image: left %.4f top %.4f width '
            '%.4f height %.4f); lay a 1920x1080 screenshot under it'
            % (SX, SY, SW, SH, SX / W * 100, SY / H * 100, SW / W * 100, SH / H * 100),
            '')


def badge(fonts, sc, spec):
    """One 88x31 button: a two-pixel bevel round a panel, as they were made."""
    W, H = 88, 31
    im = Image.new('RGBA', (W, H), (0, 0, 0, 255))
    d = ImageDraw.Draw(im)
    light = is_light(sc)
    ground = sc['deep']
    face = sc['backdrop'] if not light else sc['variant']
    hi = mix(sc['primary'], WHITE, 0.45) if not light else WHITE
    lo = mix(sc['primary'], BLACK, 0.55) if not light else mix(sc['dim'], BLACK, 0.35)
    # bevel: light top-left, dark bottom-right, then the face
    rect(d, 0, 0, W, H, lo + (255,))
    rect(d, 0, 0, W - 1, H - 1, hi + (255,))
    rect(d, 1, 1, W - 2, H - 2, face + (255,))
    kind = spec['kind']
    ink = sc['primary']
    text = sc['text']
    if kind == 'split':
        lw = spec['left_w']
        rect(d, 2, 2, lw, H - 4, ink + (255,))
        f = fonts[spec['left_font']]
        tw = f.width(spec['left'])
        f.draw(im, (2 + (lw - tw) // 2, (H - f.height) // 2), spec['left'], ground)
        x0 = 2 + lw + 3
        lines = spec['right']
        tot = sum(fonts[fk].height for fk, _, _ in lines) + (len(lines) - 1)
        y = (H - tot) // 2
        for fk, s, role in lines:
            f = fonts[fk]
            room = W - 2 - x0
            f.draw(im, (x0 + (room - f.width(s)) // 2, y), s, sc[role])
            y += f.height + 1
    elif kind == 'icon':
        ic = icon(ICONS[spec['icon']][2])
        box = Image.new('RGBA', (16, 16), ink + (0,))
        box.putalpha(ic.getchannel('A'))
        rect(d, 3, 3, 24, H - 6, ground + (255,))
        im.alpha_composite(box, (7, (H - 16) // 2))
        x0 = 30
        lines = spec['lines']
        tot = sum(fonts[fk].height for fk, _, _ in lines) + (len(lines) - 1)
        y = (H - tot) // 2
        for fk, s, role in lines:
            f = fonts[fk]
            room = W - 3 - x0
            f.draw(im, (x0 + (room - f.width(s)) // 2, y), s, sc[role])
            y += f.height + 1
    elif kind == 'ring':
        # the "inside" parody: a swoosh round a word
        lay = Image.new('L', (W * 4, H * 4), 0)
        ImageDraw.Draw(lay).ellipse((10, 8, W * 4 - 10, H * 4 - 8), outline=255, width=7)
        ImageDraw.Draw(lay).rectangle((W * 4 - 120, 0, W * 4, H * 2 - 14), fill=0)
        ring = lay.resize((W, H), Image.BOX).point(lambda v: 255 if v > 90 else 0)
        sw = Image.new('RGBA', (W, H), sc['secondary'] + (255,))
        sw.putalpha(ring)
        im.alpha_composite(sw)
        rect(d, 0, 0, W, 1, hi + (255,)); rect(d, 0, 0, 1, H, hi + (255,))
        rect(d, 0, H - 1, W, 1, lo + (255,)); rect(d, W - 1, 0, 1, H, lo + (255,))
        f = fonts['t14b']
        s = spec['word']
        f.draw(im, ((W - f.width(s)) // 2, 5), s, ink)
        f2 = fonts['f5x7']
        s2 = spec['under']
        f2.draw(im, ((W - f2.width(s2)) // 2 + 6, 19), s2, text)
    elif kind == 'band':
        # a lit title band over a caption, the plainest of the old buttons
        rect(d, 2, 2, W - 4, 15, ink + (255,))
        f = fonts[spec['font']]
        f.draw(im, ((W - f.width(spec['top'])) // 2, 2 + (15 - f.height) // 2 + 1),
               spec['top'], ground)
        f2 = fonts['f5x7']
        f2.draw(im, ((W - f2.width(spec['under'])) // 2, 20), spec['under'], text)
    return im


BADGES = [
    ('badge-from-source.png', 'paper', 'Built from source',
     {'kind': 'icon', 'icon': 'code',
      'lines': [('f5x7', 'BUILT', 'text'), ('f5x7', 'FROM', 'text'), ('f6x10', 'SOURCE', 'primary')]}),
    ('badge-no-systemd.png', 'amber', 'No systemd init: sysvinit',
     {'kind': 'split', 'left': 'NO', 'left_font': 't14b', 'left_w': 24,
      'right': [('f6x10', 'SYSTEMD', 'primary'), ('f5x7', 'INIT', 'text'),
                ('f4x6', 'SYSVINIT', 'secondary')]}),
    ('badge-musl-inside.png', 'ice', 'musl inside',
     {'kind': 'ring', 'word': 'musl', 'under': 'inside'}),
    ('badge-plasma.png', 'bone', 'KDE Plasma 6',
     {'kind': 'icon', 'icon': 'window',
      'lines': [('t12b', 'KDE', 'primary'), ('f5x7', 'PLASMA 6', 'text')]}),
    ('badge-wayland.png', 'norton', 'Wayland session',
     {'kind': 'band', 'top': 'WAYLAND', 'font': 't12b', 'under': 'SESSION'}),
    ('badge-ab-updates.png', 'borland', 'A/B updates',
     {'kind': 'icon', 'icon': 'slots',
      'lines': [('t14b', 'A/B', 'secondary'), ('f5x7', 'UPDATES', 'text')]}),
    ('badge-kpkg.png', 'perfect', 'Powered by kpkg',
     {'kind': 'icon', 'icon': 'package',
      'lines': [('f5x7', 'POWERED BY', 'text'), ('t14b', 'kpkg', 'primary')]}),
    ('badge-phosphor.png', 'phosphor', 'Best viewed in Phosphor',
     {'kind': 'icon', 'icon': 'terminal',
      'lines': [('f4x6', 'BEST VIEWED IN', 'text'), ('f6x10', 'PHOSPHOR', 'primary')]}),
]


def make_badges(art, schemes, fonts):
    by = {s['name']: s for s in schemes}
    for name, scheme, alt, spec in BADGES:
        sc = by[scheme]
        art.add(name, badge(fonts, sc, spec),
                'web button in the %s accent; show it at 1x or 2x with '
                'image-rendering: pixelated' % sc['label'], alt)


def accent_card(fonts, sc, default):
    """A scheme as a text-mode panel: its name in the frame, its swatches."""
    W, H = 200, 118
    im = Image.new('RGBA', (W, H), sc['deep'] + (255,))
    d = ImageDraw.Draw(im)
    ink, text = sc['primary'], sc['text']
    # the double frame of a text-mode panel, its title set into the top edge
    d.rectangle((2, 5, W - 3, H - 4), outline=ink)
    d.rectangle((5, 8, W - 6, H - 7), outline=ink)
    f = fonts['t14b']
    title = ' %s ' % sc['label']
    tw = f.width(title)
    tx = (W - tw) // 2
    rect(d, tx, 0, tw, f.height, ink)
    f.draw(im, (tx, 0), title, sc['deep'])

    small = fonts['t12']
    small.draw(im, (12, 17), sc['theme'], text)
    if default:
        tag = 'DEFAULT'
        tw2 = small.width(tag) + 4
        rect(d, W - 12 - tw2, 17, tw2, small.height, sc['secondary'])
        small.draw(im, (W - 12 - tw2 + 2, 17), tag, sc['deep'])

    # the three accents, big, each with its name and number
    big = [('primary', 'PRI'), ('secondary', 'SEC'), ('urgent', 'URG')]
    sx, sy, sw, sh = 12, 34, 56, 34
    tiny = fonts['f5x7']
    for i, (field, lab) in enumerate(big):
        x = sx + i * (sw + 2)
        rect(d, x, sy, sw, sh, sc[field])
        c = sc['deep'] if contrast(sc[field], sc['deep']) >= contrast(sc[field], WHITE) else WHITE
        tiny.draw(im, (x + 3, sy + 3), lab, c)
        tiny.draw(im, (x + 3, sy + sh - 10), '%02x%02x%02x' % sc[field], c)
    # the grounds and fills, small, from the deepest up to the text
    ramp = ['deep', 'backdrop', 'variant', 'dim', 'pdark', 'text']
    rw = (W - 24 - (len(ramp) - 1) * 2) // len(ramp)
    for i, field in enumerate(ramp):
        x = 12 + i * (rw + 2)
        rect(d, x, 72, rw, 14, sc[field])
        d.rectangle((x, 72, x + rw - 1, 72 + 13), outline=mix(sc['deep'], text, 0.35))
    # the command that switches to it, at a prompt
    x = small.draw(im, (12, 94), '$ ', ink)
    x = small.draw(im, (x, 94), 'kdos theme %s' % sc['name'], text)
    rect(d, x + 2, 94, 6, small.height, ink)
    return im


def make_cards(art, schemes, default, fonts):
    for sc in schemes:
        dflt = sc['name'] == default
        art.add('accent-%s.png' % sc['name'], nearest(accent_card(fonts, sc, dflt), 2),
                'swatch card for the %s accent (%s)%s: name, the three accents, the '
                'grounds and fills, its text on its ground'
                % (sc['label'], sc['theme'], ', the default' if dflt else ''),
                'The %s accent%s: swatches of its colours' % (
                    sc['label'].capitalize(), ', the default' if dflt else ''))


def crt_screen(sc, size, glow_from):
    """A lit tube in the scheme: its ground and a wash of pdark spreading
    from glow_from. The vignette and the scanlines go on last, over the
    content, as the glass is in front of the picture."""
    W, H = size
    im = Image.new('RGBA', (W, H), sc['deep'] + (255,))
    wash = Image.new('RGBA', (W, H), sc['pdark'] + (0,))
    m = Image.new('L', (W, H), 0)
    gx, gy = glow_from
    ImageDraw.Draw(m).ellipse((gx - 600, gy - 450, gx + 600, gy + 450), fill=70)
    wash.putalpha(m.filter(ImageFilter.GaussianBlur(160)))
    im.alpha_composite(wash)
    return im


def lit(im, sc, xy, content, radii):
    """Lay content on im at xy with a halo of the accent round it."""
    pad = 2 * max(r for r, _ in radii)
    lay = Image.new('RGBA', (content.width + 2 * pad, content.height + 2 * pad), (0, 0, 0, 0))
    lay.alpha_composite(content, (pad, pad))
    im.alpha_composite(glow(lay, sc['primary'], radii), (xy[0] - pad, xy[1] - pad))


def text_block(fonts, sc, lines, width):
    """Lines of (font, text, colour, gap above), a block cursor after the last."""
    h = sum(fonts[f].height + gap for f, _, _, gap in lines)
    im = Image.new('RGBA', (width, h + 4), (0, 0, 0, 0))
    y = x = 0
    for f, t, colour, gap in lines:
        y += gap
        x = fonts[f].draw(im, (0, y), t, colour)
        y += fonts[f].height
    f = fonts[lines[-1][0]]
    ImageDraw.Draw(im).rectangle((x, y - f.height + 2, x + f.width('M') - 1, y - 1),
                                 fill=lines[-1][2])
    return im


def trimmed_penguin(idx, sc, scale):
    p = splash_penguin(idx, sc, 1)
    return nearest(p.crop(p.getbbox()), scale)


def make_social(art, schemes, words, idx, fonts):
    """The 1200x630 card a link to the site unfurls into, and the README's
    banner: the same lit tube in Phosphor, two shapes."""
    sc = next(s for s in schemes if s['name'] == 'phosphor')
    word = nearest(wordmark(words, sc, cw=10, ch=20, t=1, gap=2), 2)
    peng = trimmed_penguin(idx, sc, 2)
    soft = mix(sc['text'], sc['deep'], 0.12)

    W, H = 1200, 630
    im = crt_screen(sc, (W, H), (300, 250))
    lit(im, sc, (64, 64), word, ((2, 0.9), (8, 0.55), (22, 0.45)))
    lit(im, sc, (W - peng.width - 72, (H - peng.height) // 2), peng, ((6, 0.25), (24, 0.25)))
    txt = text_block(fonts, sc, [
        ('t32b', NAME, sc['text'], 0),
        ('t24b', BLURB, soft, 14),
        ('t24b', STACK, sc['secondary'], 12),
        ('t24b', '> ' + SITE + ' ', sc['primary'], 26)], 640)
    lit(im, sc, (68, 64 + word.height + 34), txt, ((3, 0.18),))
    im.alpha_composite(vignette((W, H), BLACK, 0.55, 150))
    im = scanlines(im, 3, 46)
    art.add('social-card.png', im.convert('RGB'),
            'OpenGraph and Twitter card, opaque: the wordmark, the '
            'penguin, the name, the stack and the address, in the Phosphor accent',
            'KDOS, Kool Desktop Operating System: a Linux distribution compiled '
            'from source; musl, GNU, sysvinit, KDE Plasma. A green phosphor '
            'wordmark beside the KDOS penguin.')

    W, H = 1280, 420
    im = crt_screen(sc, (W, H), (360, 160))
    lit(im, sc, (60, 44), word, ((2, 0.9), (8, 0.55), (22, 0.45)))
    lit(im, sc, (W - peng.width - 96, (H - peng.height) // 2), peng, ((6, 0.25), (24, 0.25)))
    txt = text_block(fonts, sc, [
        ('t32b', NAME, sc['text'], 0),
        ('t24b', STACK + ' ', sc['secondary'], 10)], 700)
    lit(im, sc, (64, 44 + word.height + 22), txt, ((3, 0.18),))
    im.alpha_composite(vignette((W, H), BLACK, 0.6, 140))
    im = scanlines(im, 3, 46)
    corner = Image.new('L', (W, H), 0)
    ImageDraw.Draw(corner).rounded_rectangle((0, 0, W - 1, H - 1), 22, fill=255)
    im.putalpha(corner)
    art.add('readme-hero.png', im,
            'the README\'s banner, rounded corners on transparency so '
            'it sits on a light or a dark page: the wordmark, the penguin, the '
            'name and the stack, in the Phosphor accent',
            'KDOS, Kool Desktop Operating System: musl, GNU, sysvinit, KDE Plasma')


def penguin_cut(master):
    """kdos.png's penguin, without the wordmark below it, cropped square.

    The penguin and the wordmark are two blocks of opaque pixels with a band
    of glow alone between them, the split genmarks.py finds for the tux mark.
    The cut is halfway across that band, and the glow fades out from the
    penguin's last solid row to the cut: a cut through lit glow is a hard
    green edge under the feet.
    """
    a = master.getchannel('A')
    w, h = master.size
    rows = [a.crop((0, y, w, y + 1)).getextrema()[1] >= 150 for y in range(h)]
    y = rows.index(True)
    while y < h and rows[y]:
        y += 1
    end = y
    while y < h and not rows[y]:
        y += 1
    cut = (end + y) // 2
    band = master.crop((0, 0, w, cut))
    fade = Image.new('L', (1, cut), 255)
    for r in range(end, cut):
        fade.putpixel((0, r), int(255 * (cut - r) / (cut - end)))
    band.putalpha(ImageChops.multiply(band.getchannel('A'), fade.resize((w, cut))))
    bb = band.getchannel('A').point(lambda v: 255 if v >= 8 else 0).getbbox()
    pen = band.crop(bb)
    side = max(pen.size)
    sq = Image.new('RGBA', (side, side), (0, 0, 0, 0))
    sq.alpha_composite(pen, ((side - pen.width) // 2, (side - pen.height) // 2))
    return sq


def tile_icon(sc, pen, size, radius_frac=0.18, fill_frac=0.86):
    """The penguin on a rounded tile of the scheme's ground, for launchers."""
    S = 4
    big = size * S
    im = Image.new('RGBA', (big, big), (0, 0, 0, 0))
    ImageDraw.Draw(im).rounded_rectangle((0, 0, big - 1, big - 1), int(big * radius_frac),
                                         fill=sc['deep'] + (255,))
    p = pen.resize((int(big * fill_frac),) * 2, Image.LANCZOS)
    im.alpha_composite(p, ((big - p.width) // 2, (big - p.height) // 2))
    return im.resize((size, size), Image.LANCZOS)


def make_icons(art, schemes, default, master):
    sc = next(s for s in schemes if s['name'] == default)
    marks = {s: load_rgba(os.path.join(MARKS, 'tux-%d.png' % s)) for s in (16, 32, 48)}
    buf = io.BytesIO()
    marks[48].save(buf, 'ICO', sizes=[(16, 16), (32, 32), (48, 48)],
                   append_images=[marks[16], marks[32]])
    art.add('favicon.ico', buf.getvalue(),
            'favicon: the tux marks of kdos-icons at 16, 32 and 48', '')
    art.add('favicon-32.png', marks[32], 'favicon as PNG: the tux mark', '')
    pen = penguin_cut(master)
    art.add('icon-192.png', tile_icon(sc, pen, 192),
            'web app icon: the penguin on a rounded tile of the default accent\'s ground', '')
    # iOS rounds the corners itself and shows transparency as black
    art.add('apple-touch-icon.png', tile_icon(sc, pen, 180, 0.0, 0.86).convert('RGB'),
            'apple-touch-icon, square and opaque: the penguin on the default '
            'accent\'s ground', '')


def make_pixel_icons(art):
    for key, (what, use, rows) in ICONS.items():
        art.add('pixel-%s.png' % key, nearest(icon(rows), 4),
                'pixel icon, drawn at 16x16 and shipped at 4x, white on transparent: '
                '%s; for %s; tint with CSS mask-image' % (what, use), '')


def make_ornaments(art):
    # a rule that dithers out to nothing at both ends, ordered (Bayer)
    bayer = [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]
    W = 240
    r = Image.new('RGBA', (W, 4), (255, 255, 255, 0))
    for x in range(W):
        v = 1.0 - abs(x - (W - 1) / 2) / ((W - 1) / 2)
        v = min(1.0, v * 1.6)
        for y in range(2):
            if v * 16 > bayer[y % 4][x % 4]:
                r.putpixel((x, y + 1), (255, 255, 255, 255))
    art.add('rule-dither.png', nearest(r, 2),
            'section rule: a two-pixel line that dithers out at both '
            'ends, white on transparent', '')

    # a bullet: the right-pointing triangle of DOS menus
    b = Image.new('RGBA', (8, 8), (255, 255, 255, 0))
    db = ImageDraw.Draw(b)
    for i in range(4):
        rect(db, 1 + i, i, 1, 8 - 2 * i, (255, 255, 255, 255))
    art.add('bullet.png', nearest(b, 2),
            'list bullet: a text-mode right-pointing triangle, white on '
            'transparent', '')


# ── fonts ────────────────────────────────────────────────────────────────
#
# Terminus is the pixel face (Phosphor mode, the hero, headings at 16, 24 or
# 32 px, the sizes its strikes are drawn at); JetBrains Mono the code face.
# Both are under the SIL Open Font License. Terminus carries Reserved Font
# Names, so it is converted to WOFF2 whole and with no metadata added: the
# OFL FAQ counts WOFF compression of unaltered font data as no modification,
# and a subset would be a Modified Version that may not keep the name. JetBrains Mono reserves no name and is
# subset to Latin, punctuation, arrows, box drawing and blocks, and the three
# symbols the page's terminals draw (the prompt's λ, and □ and ✕ in a title
# bar, which Terminus lacks), without its
# hinting and its OpenType features: the code ligatures would turn `->` and
# `!=` in a command into glyphs a reader cannot type back.

WEBFONTS = [
    ('terminus.woff2', 'TTF/TerminusTTF-4.49.3.ttf', None),
    ('terminus-bold.woff2', 'TTF/TerminusTTF-Bold-4.49.3.ttf', None),
    ('jetbrains-mono.woff2', 'TTF/JetBrainsMono-Regular.ttf', 'latin'),
    ('jetbrains-mono-bold.woff2', 'TTF/JetBrainsMono-Bold.ttf', 'latin'),
]
FONT_LICENCES = [
    ('LICENSE-Terminus.txt', '../licenses/terminus-ttf/COPYING'),
    ('LICENSE-JetBrainsMono.txt', '../licenses/ttf-jetbrains-mono/OFL.txt'),
]
LATIN = (list(range(0x20, 0x7f)) + list(range(0xa0, 0x180)) +
         list(range(0x2010, 0x2028)) + list(range(0x2030, 0x2050)) +
         [0x20ac, 0x2122, 0x2212] + list(range(0x2190, 0x2200)) +
         list(range(0x2500, 0x25a0)) + [0x25b2, 0x25b6, 0x25bc, 0x25c0, 0x25c6, 0x25cf] +
         [0x03bb, 0x25a1, 0x2715])


def write_fonts(fontdir, check):
    try:
        from fontTools.ttLib import TTFont
        from fontTools import subset
        import brotli  # noqa: F401  (WOFF2 needs it)
    except ImportError:
        raise SystemExit('genart: --fonts needs fontTools and brotli; see the header')
    drift = []
    if not check:
        os.makedirs(FONTS_OUT, exist_ok=True)
    for out, src, sub in WEBFONTS:
        path = os.path.join(fontdir, src)
        if sub:
            opts = subset.Options()
            opts.layout_features = []
            opts.hinting = False
            opts.name_IDs = ['*']
            opts.notdef_outline = True
            opts.glyph_names = False
            f = subset.load_font(path, opts)
            s = subset.Subsetter(opts)
            s.populate(unicodes=LATIN)
            s.subset(f)
            f.flavor = 'woff2'
        else:
            f = TTFont(path, recalcTimestamp=False, recalcBBoxes=False)
            f.flavor = 'woff2'
        buf = io.BytesIO()
        f.save(buf, reorderTables=True)
        data = buf.getvalue()
        dest = os.path.join(FONTS_OUT, out)
        if check:
            if not same_bytes(dest, data):
                drift.append(dest)
        else:
            with open(dest, 'wb') as fo:
                fo.write(data)
    for out, rel in FONT_LICENCES:
        with open(os.path.normpath(os.path.join(fontdir, rel)), 'rb') as fi:
            data = fi.read()
        dest = os.path.join(FONTS_OUT, out)
        if check:
            if not same_bytes(dest, data):
                drift.append(dest)
        else:
            with open(dest, 'wb') as fo:
                fo.write(data)
    return drift


# ── main ─────────────────────────────────────────────────────────────────

def draw_all(fontdir):
    schemes, default = read_schemes(KCOLOR)
    words = read_wordmark(LOGO)
    idx = read_penguin(PENGUIN)
    master = load_rgba(MASTER)
    fonts = load_fonts(fontdir)
    art = Art()
    make_banners(art, schemes, words)
    make_penguins(art, schemes, idx)
    make_social(art, schemes, words, idx, fonts)
    make_cards(art, schemes, default, fonts)
    make_badges(art, schemes, fonts)
    make_bezel(art, fonts)
    make_ornaments(art)
    make_pixel_icons(art)
    make_icons(art, schemes, default, master)
    return art


# THE INPUTS ARE RECORDED, so a change to one is caught where Pillow and the
# fonts are not at hand. inputs.tsv holds the SHA-256 of each tree input the
# art is drawn from; script/site/mksite.py computes the same and stops when
# one differs, which `make sitecheck`, `make check` and the site's deploy
# run. The palette is the KCOL_SCHEMES rows as schemes_text() writes them, not
# the whole of kcolor.h, so an edit elsewhere in the header asks for no
# redraw. The fonts are build/fs's and are not recorded: the check runs where
# no built tree is.
INPUT_FILES = (LOGO, PENGUIN, MASTER) + tuple(
    os.path.join(MARKS, 'tux-%d.png' % s) for s in (16, 32, 48))
SCHEMES_INPUT = 'src/libs/libkcolor/kcolor.h KCOL_SCHEMES'


def schemes_text(schemes, default):
    """The palette as recorded: a line per scheme, name, label, theme and
    its nine colours in lowercase hex, then the default. mksite.py's
    schemes_text() writes the same from its own reading of the header."""
    rows = ['%s\t%s\t%s\t%s\n' % (s['name'], s['label'], s['theme'],
                                  ','.join('%02x%02x%02x' % s[f] for f in KCOL_FIELDS))
            for s in schemes]
    return ''.join(rows) + 'default\t%s\n' % default


def inputs_tsv():
    """input, sha256 for every tree input the art is drawn from."""
    schemes, default = read_schemes(KCOLOR)
    lines = ['input\tsha256',
             '%s\t%s' % (SCHEMES_INPUT,
                         hashlib.sha256(schemes_text(schemes, default).encode()).hexdigest())]
    for path in INPUT_FILES:
        with open(path, 'rb') as f:
            lines.append('%s\t%s' % (os.path.relpath(path, ROOT),
                                     hashlib.sha256(f.read()).hexdigest()))
    return ('\n'.join(lines) + '\n').encode('utf-8')


def tsv(art):
    """file, purpose, alt. The purpose opens with the pixel size the page
    gives as width and height; an empty alt is a decorative image."""
    lines = ['file\tpurpose\talt']
    for name, im, purpose, alt in art.items:
        if isinstance(im, bytes):
            im = Image.open(io.BytesIO(im))
        lines.append('%s\t%dx%d: %s\t%s' % (name, im.width, im.height, purpose, alt))
    return ('\n'.join(lines) + '\n').encode('utf-8')


def same_bytes(path, data):
    if not os.path.exists(path):
        return False
    with open(path, 'rb') as f:
        return f.read() == data


def same_pixels(im, path):
    """im (an image, or the bytes of one) has the pixels of the file at path."""
    if isinstance(im, bytes):
        im = Image.open(io.BytesIO(im))
    with Image.open(path) as f:
        b = f.convert('RGBA')
    a = im.convert('RGBA')
    # compared as bytes: getbbox() on an RGBA difference reads the alpha
    # alone, and two accents of one banner share every alpha value
    return a.size == b.size and a.tobytes() == b.tobytes()


def main():
    ap = argparse.ArgumentParser(description='Draw the kdos.dev artwork.')
    ap.add_argument('--check', action='store_true',
                    help='redraw in memory and report files that differ')
    ap.add_argument('--fonts', action='store_true',
                    help='also write the web fonts (needs fontTools and brotli)')
    ap.add_argument('--fontdir', default=FONTDIR,
                    help='the fonts directory of a built tree (default: build/fs)')
    a = ap.parse_args()

    art = draw_all(a.fontdir)
    drift = []
    if not a.check:
        os.makedirs(OUT, exist_ok=True)
    for name, im, _, _ in art.items:
        dest = os.path.join(OUT, name)
        if a.check:
            if not os.path.exists(dest):
                drift.append(dest)
            elif isinstance(im, bytes):
                if name.endswith('.ico'):
                    if not same_pixels(im, dest):
                        drift.append(dest)
                elif not same_bytes(dest, im):
                    drift.append(dest)
            elif not same_pixels(im, dest):
                drift.append(dest)
        else:
            with open(dest, 'wb') as f:
                f.write(im if isinstance(im, bytes) else png_bytes(im))
    for name, table in (('art.tsv', tsv(art)), ('inputs.tsv', inputs_tsv())):
        dest = os.path.join(OUT, name)
        if a.check:
            if not same_bytes(dest, table):
                drift.append(dest)
        else:
            with open(dest, 'wb') as f:
                f.write(table)
    n = len(art.items) + 2
    if a.fonts:
        drift += write_fonts(a.fontdir, a.check)
        n += len(WEBFONTS) + len(FONT_LICENCES)
    # a file nothing draws is artwork nobody can regenerate
    known = {name for name, _, _, _ in art.items} | {'art.tsv', 'inputs.tsv'}
    stale = sorted(set(os.listdir(OUT)) - known) if os.path.isdir(OUT) else []
    for name in stale:
        print('genart: not drawn by this script: site/assets/art/%s' % name)

    if a.check:
        for p in drift:
            print('genart: differs: %s' % os.path.relpath(p, ROOT))
        print('genart: %d files, %d differ, %d not drawn' % (n, len(drift), len(stale)))
        return 1 if drift or stale else 0
    print('genart: %d files -> %s' % (n, os.path.relpath(OUT, ROOT)))
    return 0


if __name__ == '__main__':
    sys.exit(main())
