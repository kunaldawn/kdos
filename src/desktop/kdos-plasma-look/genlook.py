#!/usr/bin/env python3
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   genlook — the KDOS global themes, Plasma style and previews, at build time
#
#     genlook.py --src PORT_SRC --pkg PKG --schemes DIR --splash DIR \
#                --accents NAME...
#
#   PORT_SRC  this port: lookandfeel/ (the files every global theme
#             carries), desks/ and desktoptheme/
#   PKG       the package root everything is written under
#   DIR       --schemes: the KDOS<Title>.colors files kdos-theme wrote;
#             --splash: src/art/kdos-splash, whose penguin.h, wordmark and
#             mascot palette the splash and the previews draw
#   NAME      every accent, in libkcolor's table order (`kdos-theme accents`)
#
# EVERY COLOUR HERE IS READ FROM A COLOUR SCHEME FILE. The accents, their
# names and which of them are light come from the .colors files kdos-theme
# generated from libkcolor, and nothing in this program spells one: a second
# copy of the palette is the copy that goes stale when an accent moves. The
# values that are not a scheme's are the mascot's own, its body and the colour
# its rim is mixed towards, both read out of kdos-splash.c so the two splashes
# agree; and SCREEN_OFF, the black of a screen with nothing on it, which every
# handover of the power-on passes through and no accent may own.
#
# Everything is written deterministically (sorted keys, fixed PNG and gzip
# headers, no timestamps), so two builds of one tree install the same bytes.
# ---------------------------------

import argparse
import gzip
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import zlib

# ─── colour schemes ────────────────────────────────────────────────────────


def die(msg):
    print("genlook: " + msg, file=sys.stderr)
    sys.exit(1)


def read_ini(path):
    """KConfig-shaped file as {group: {key: value}}; nested groups keep their
    KConfig spelling ("Colors:Header][Inactive"). Localised keys are
    distinct keys and are kept as written."""
    groups = {}
    cur = None
    with open(path, encoding="utf-8") as f:
        for raw in f:
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            if line.startswith("[") and line.endswith("]"):
                cur = groups.setdefault(line[1:-1], {})
                continue
            if cur is None or "=" not in line:
                continue
            k, v = line.split("=", 1)
            cur[k.strip()] = v.strip()
    return groups


def rgb(value, where):
    parts = value.split(",")
    if len(parts) < 3:
        die("%s: '%s' is not R,G,B" % (where, value))
    try:
        return tuple(max(0, min(255, int(p))) for p in parts[:3])
    except ValueError:
        die("%s: '%s' is not R,G,B" % (where, value))


def spl_mix(a, b, pct):
    """`pct` percent of the way from a to b, as kcol_mix and kdos-splash's
    spl_mix compute it: the unsigned weighted sum, floored once."""
    return tuple((a[i] * (100 - pct) + b[i] * pct) // 100 for i in range(3))


def mix_rgb(a, b, t):
    """kdos-splash's mix_rgb: a blend by the fraction `t`, truncated."""
    return tuple(int(a[i] * (1.0 - t) + b[i] * t) for i in range(3))


def hexc(c):
    return "#%02x%02x%02x" % c


# The black of a screen with nothing on it. It is not a scheme role and must
# not become one: every stage of the power-on ends on it and the next starts
# from it, so a mode set falls inside a black frame whatever the accent.
SCREEN_OFF = (0, 0, 0)


class Scheme:
    """One accent, as its generated .colors file states it."""

    def __init__(self, accent, path):
        self.accent = accent
        self.path = path
        g = read_ini(path)
        gen = g.get("General", {})
        self.scheme = os.path.basename(path)[: -len(".colors")]
        name = gen.get("Name", "")
        if not name.startswith("KDOS "):
            die("%s: [General] Name is '%s', not 'KDOS <Title>'" % (path, name))
        self.title = name[len("KDOS "):]
        if gen.get("ColorScheme", self.scheme) != self.scheme:
            die("%s: [General] ColorScheme does not match the file name" % path)

        def role(group, key):
            v = g.get(group, {}).get(key)
            if v is None:
                die("%s: [%s] %s is missing" % (path, group, key))
            return rgb(v, "%s [%s] %s" % (path, group, key))

        self.window_bg = role("Colors:Window", "BackgroundNormal")
        self.window_alt = role("Colors:Window", "BackgroundAlternate")
        self.window_fg = role("Colors:Window", "ForegroundNormal")
        self.window_inactive = role("Colors:Window", "ForegroundInactive")
        self.window_hover = role("Colors:Window", "DecorationHover")
        self.window_focus = role("Colors:Window", "DecorationFocus")
        self.neutral = role("Colors:Window", "ForegroundNeutral")
        self.negative = role("Colors:Window", "ForegroundNegative")
        self.positive = role("Colors:Window", "ForegroundPositive")
        self.view_bg = role("Colors:View", "BackgroundNormal")
        self.view_alt = role("Colors:View", "BackgroundAlternate")
        self.view_fg = role("Colors:View", "ForegroundNormal")
        self.view_inactive = role("Colors:View", "ForegroundInactive")
        self.button_bg = role("Colors:Button", "BackgroundNormal")
        self.button_fg = role("Colors:Button", "ForegroundNormal")
        self.button_hover = role("Colors:Button", "DecorationHover")
        self.button_focus = role("Colors:Button", "DecorationFocus")
        self.sel_bg = role("Colors:Selection", "BackgroundNormal")
        self.sel_fg = role("Colors:Selection", "ForegroundNormal")
        self.tip_bg = role("Colors:Tooltip", "BackgroundNormal")
        self.tip_fg = role("Colors:Tooltip", "ForegroundNormal")
        self.header_bg = role("Colors:Header", "BackgroundNormal")
        self.header_fg = role("Colors:Header", "ForegroundNormal")

    @property
    def light(self):
        # KLookAndFeel::colorSchemeVariant's rule, so KDOS and the Global
        # Theme page agree on which accents are light: qGray() of the Window
        # background, light from 192.
        r, g, b = self.window_bg
        return (r * 11 + g * 16 + b * 5) // 32 >= 192


def load_schemes(schemes_dir, accents):
    out = []
    for a in accents:
        if not re.fullmatch(r"[a-z][a-z0-9]*", a):
            die("accent '%s' is not a lower-case name" % a)
        path = os.path.join(schemes_dir, "KDOS" + a[0].upper() + a[1:] + ".colors")
        if not os.path.isfile(path):
            die("%s: no colour scheme for accent '%s'" % (path, a))
        out.append(Scheme(a, path))
    if not out:
        die("no accents given")
    return out


# ─── the mascot and the wordmark, read out of kdos-splash ─────────────────


def hex_rgb(text):
    v = int(text, 16)
    return ((v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF)


class Mark:
    """The penguin (palette-indexed RLE), the block-art wordmark and the
    mascot's own colours, read from kdos-splash's sources so the splash this
    program writes and the boot splash draw one picture. A source this cannot
    parse fails the build; a splash with no mascot is not a fallback."""

    def __init__(self, splash_dir):
        hdr = open(os.path.join(splash_dir, "penguin.h"), encoding="utf-8").read()
        m_w = re.search(r"#define\s+PENGUIN_W\s+(\d+)", hdr)
        m_h = re.search(r"#define\s+PENGUIN_H\s+(\d+)", hdr)
        m_d = re.search(r"penguin_rle\[\]\s*=\s*\{([^}]*)\}", hdr, re.S)
        if not (m_w and m_h and m_d):
            die("penguin.h: no PENGUIN_W, PENGUIN_H and penguin_rle")
        self.w, self.h = int(m_w.group(1)), int(m_h.group(1))
        self.rle = [int(x) for x in re.findall(r"\d+", m_d.group(1))]
        if len(self.rle) % 2:
            die("penguin.h: penguin_rle has an odd length")
        self.grid = self._decode()

        src = open(os.path.join(splash_dir, "kdos-splash.c"), encoding="utf-8").read()
        m_art = re.search(r"static const char \*art\[ART_ROWS\]\s*=\s*\{(.*?)\};", src, re.S)
        if not m_art:
            die("kdos-splash.c: no art[] wordmark")
        self.art = re.findall(r'"([^"]*)"', m_art.group(1))
        if not self.art or len({len(r) for r in self.art}) != 1:
            die("kdos-splash.c: the art[] rows are not one width")
        m_sub = re.search(r'const char \*sub\s*=\s*"([^"]*)"', src)
        m_tag = re.search(r'const char \*tag\s*=\s*"([^"]*)"', src)
        if not (m_sub and m_tag):
            die("kdos-splash.c: no sub or tag line")
        self.sub, self.tag = m_sub.group(1), m_tag.group(1)

        # peng_pal[1] is the mascot's body; its rim, C_GLOW, is the scheme's
        # text mixed towards one fixed colour; the wordmark's lower rows are
        # mixed towards the dim colour by one fixed fraction.
        m_body = re.search(r"peng_pal\[1\]\s*=\s*0x([0-9a-fA-F]{6})\s*;", src)
        m_glow = re.search(r"C_GLOW\s*=\s*spl_mix\(\s*s->text\s*,\s*0x([0-9a-fA-F]{6})\s*,\s*(\d+)\s*\)", src)
        m_low = re.search(r"col\s*=\s*mix_rgb\(\s*bright\s*,\s*dim\s*,\s*([0-9.]+)\s*\)", src)
        if not (m_body and m_glow and m_low):
            die("kdos-splash.c: no peng_pal[1], C_GLOW or wordmark mix to read the mascot from")
        self.body = hex_rgb(m_body.group(1))
        self.glow_to = hex_rgb(m_glow.group(1))
        self.glow_pct = int(m_glow.group(2))
        self.art_lower = float(m_low.group(1))
        if not (0 <= self.glow_pct <= 100 and 0.0 <= self.art_lower <= 1.0):
            die("kdos-splash.c: the mascot's mix values are out of range")

    def _decode(self):
        grid = []
        row = []
        for i in range(0, len(self.rle), 2):
            idx, run = self.rle[i], self.rle[i + 1]
            row.extend([idx] * run)
            if len(row) >= self.w:
                if len(row) != self.w:
                    die("penguin.h: a run crosses a row boundary")
                grid.append(row)
                row = []
        if row or len(grid) != self.h:
            die("penguin.h: %d rows decoded, PENGUIN_H is %d" % (len(grid), self.h))
        return grid

    def rects(self):
        """The penguin as filled rectangles in its own pixels, (x, y, w, h,
        palette index): each row's runs, with a run that repeats on the next
        row at the same place and colour merged into one taller rectangle. The
        splash draws one QML item per rectangle, so merging is what keeps it a
        few hundred items rather than thousands."""
        open_runs = {}
        done = []
        for y, row in enumerate(self.grid):
            runs = set()
            x = 0
            while x < self.w:
                idx = row[x]
                start = x
                while x < self.w and row[x] == idx:
                    x += 1
                if idx:
                    runs.add((start, x - start, idx))
            for key in list(open_runs):
                if key not in runs:
                    y0 = open_runs.pop(key)
                    done.append((key[0], y0, key[1], y - y0, key[2]))
            for key in runs:
                open_runs.setdefault(key, y)
        for key, y0 in open_runs.items():
            done.append((key[0], y0, key[1], self.h - y0, key[2]))
        done.sort(key=lambda r: (r[1], r[0]))
        return done

    def qml_js(self):
        """The splash's data module: one source, so Splash.qml cannot carry
        a second copy of the picture or of the mascot's own colours."""
        rows = ",\n    ".join(json.dumps(r) for r in self.art)
        flat = ",".join("%d,%d,%d,%d,%d" % r for r in self.rects())
        return (
            ".pragma library\n\n"
            "// Generated by genlook.py from src/art/kdos-splash (penguin.h and\n"
            "// kdos-splash.c); edit those, not this file.\n\n"
            "var penguinWidth = %d;\n"
            "var penguinHeight = %d;\n"
            "// x, y, width, height, palette index: five numbers a rectangle.\n"
            "var penguinRects = [%s];\n\n"
            "var art = [\n    %s\n];\n\n"
            "var subtitle = %s;\n"
            "var tagline = %s;\n\n"
            "// The mascot's own colours, which no scheme owns: palette index 1,\n"
            "// and the colour index 2 mixes the scheme's text towards.\n"
            "var mascotBody = %s;\n"
            "var glowTowards = %s;\n"
            "var glowFraction = %s;\n"
            "// How far the wordmark's lower rows go towards the dim colour.\n"
            "var artLowerFraction = %s;\n\n"
            "// A screen with nothing on it, where every handover starts and ends.\n"
            "var screenOff = %s;\n"
        ) % (self.w, self.h, flat, rows, json.dumps(self.sub), json.dumps(self.tag),
             json.dumps(hexc(self.body)), json.dumps(hexc(self.glow_to)),
             fmt(self.glow_pct / 100.0), fmt(self.art_lower), json.dumps(hexc(SCREEN_OFF)))


# ─── kdos-splash's layout, in pixels ──────────────────────────────────────

# kdos-splash.c draws with an 8x16 PSF font and stacks its screen from the top
# in glyph cells; these are its constants. Splash.qml repeats the same integer
# arithmetic in device pixels, and a change to layout() in kdos-splash.c is a
# change to both, or the handover jumps.
GLYPH_W, GLYPH_H = 8, 16
LINE_COLS = 40
BAR_SEGS = 26


def splash_layout(fbw, fbh, mark):
    body_scale = max(1, min(3, fbw // 640))
    cell = GLYPH_H * body_scale
    art_cw = max(4, min(60, fbw // 85))
    art_ch = art_cw * 2
    art_lt = max(1, art_cw // 3)
    art_cols, art_rows = len(mark.art[0]), len(mark.art)
    art_w, art_h = art_cols * art_cw, art_rows * art_ch
    peng_scale = max(1, (art_h * 115 // 100 + mark.h // 2) // mark.h)
    peng_w, peng_h = mark.w * peng_scale, mark.h * peng_scale
    gap = art_cw * 2
    group_w = peng_w + gap + art_w
    group_h = max(peng_h, art_h)
    peng_x = (fbw - group_w) // 2
    peng_y = fbh * 9 // 100
    art_x = peng_x + peng_w + gap
    art_y = peng_y + (group_h - art_h) // 2
    sub_y = peng_y + group_h + cell
    tag_y = sub_y + cell * 2
    rule_y = tag_y + cell * 2
    body_step = cell + body_scale * 4
    body_w = (LINE_COLS + 6) * GLYPH_W * body_scale
    body_x = max(0, (fbw - body_w) // 2)
    bar_y = rule_y + body_step
    return dict(body_scale=body_scale, cell=cell, art_cw=art_cw, art_ch=art_ch,
                art_lt=art_lt, art_x=art_x, art_y=art_y, peng_scale=peng_scale,
                peng_x=peng_x, peng_y=peng_y, sub_y=sub_y, tag_y=tag_y,
                rule_y=rule_y, body_x=body_x, body_w=body_w, bar_y=bar_y)


# ─── a raster, a PNG and a JPEG ───────────────────────────────────────────


class Raster:
    def __init__(self, w, h, ground):
        self.w, self.h = w, h
        self.px = bytearray(bytes(ground) * (w * h))

    def fill(self, x, y, w, h, c):
        x0, y0 = max(0, int(x)), max(0, int(y))
        x1, y1 = min(self.w, int(x + w)), min(self.h, int(y + h))
        if x1 <= x0 or y1 <= y0:
            return
        run = bytes(c) * (x1 - x0)
        stride = self.w * 3
        for yy in range(y0, y1):
            off = yy * stride + x0 * 3
            self.px[off:off + len(run)] = run

    def png(self):
        stride = self.w * 3
        raw = bytearray()
        for y in range(self.h):
            raw.append(0)
            raw += self.px[y * stride:(y + 1) * stride]

        def chunk(kind, data):
            body = kind + data
            return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)

        return (b"\x89PNG\r\n\x1a\n"
                + chunk(b"IHDR", struct.pack(">IIBBBBB", self.w, self.h, 8, 2, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(bytes(raw), 9))
                + chunk(b"IEND", b""))

    def ppm(self):
        return b"P6\n%d %d\n255\n" % (self.w, self.h) + bytes(self.px)

    def downscale(self, n):
        """The raster an n-th of the size, each pixel the rounded mean of the
        n by n block it covers: the small previews are drawn at three times
        their size and reduced, so one-pixel rules survive as a tint instead
        of vanishing or doubling at a third of a pixel."""
        w, h = self.w // n, self.h // n
        out = Raster(w, h, (0, 0, 0))
        stride = self.w * 3
        area = n * n
        for y in range(h):
            row = bytearray(w * 3)
            for c in range(3):
                parts = []
                for dy in range(n):
                    base = (y * n + dy) * stride
                    for dx in range(n):
                        parts.append(self.px[base + dx * 3 + c:base + w * n * 3:3 * n])
                row[c::3] = bytes((sum(t) + area // 2) // area for t in zip(*parts))
            out.px[y * w * 3:(y + 1) * w * 3] = row
        return out


def write_bytes(path, data):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(data)


def write_text(path, text):
    write_bytes(path, text.encode("utf-8"))


def write_jpeg(path, raster, scratch):
    """cjpeg from libjpeg-turbo, run with an argument vector: the KDE lookup
    names the file fullscreenpreview.jpg, so it has to be a JPEG."""
    ppm = os.path.join(scratch, os.path.basename(path) + ".ppm")
    write_bytes(ppm, raster.ppm())
    os.makedirs(os.path.dirname(path), exist_ok=True)
    try:
        subprocess.run(["cjpeg", "-quality", "92", "-optimize", "-outfile", path, ppm], check=True)
    except (OSError, subprocess.CalledProcessError) as e:
        die("cjpeg failed for %s: %s" % (path, e))
    os.unlink(ppm)


# ─── drawing the mark ─────────────────────────────────────────────────────


def mascot_palette(sc, mark):
    """kdos-splash's peng_pal in this scheme's roles: index 1 is the mascot's
    body, 2 its rim (the text mixed towards the mascot's glow colour), 3
    secondary, 4 the accent (the Window DecorationFocus), 5 the accent's dark
    (the Window DecorationHover)."""
    return {
        1: mark.body,
        2: spl_mix(sc.window_fg, mark.glow_to, mark.glow_pct),
        3: sc.neutral,
        4: sc.window_focus,
        5: sc.window_hover,
    }


def draw_penguin(r, mark, x, y, w, h, pal):
    if w <= 0 or h <= 0:
        return
    for ty in range(h):
        row = mark.grid[ty * mark.h // h]
        tx = 0
        while tx < w:
            idx = row[tx * mark.w // w]
            start = tx
            tx += 1
            while tx < w and row[tx * mark.w // w] == idx:
                tx += 1
            if idx:
                r.fill(x + start, y + ty, tx - start, 1, pal[idx])


def draw_art_cell(r, x, y, ch, cw, chh, lt, c):
    mx = x + cw // 2 - lt // 2
    my = y + chh // 2 - lt // 2
    if ch == "#":
        r.fill(x, y, cw, chh, c)
    elif ch == "=":
        r.fill(x, my, cw, lt, c)
    elif ch == "|":
        r.fill(mx, y, lt, chh, c)
    elif ch == "{":
        r.fill(mx, my, x + cw - mx, lt, c)
        r.fill(mx, my, lt, y + chh - my, c)
    elif ch == "}":
        r.fill(x, my, mx + lt - x, lt, c)
        r.fill(mx, my, lt, y + chh - my, c)
    elif ch == "[":
        r.fill(mx, my, x + cw - mx, lt, c)
        r.fill(mx, y, lt, my + lt - y, c)
    elif ch == "]":
        r.fill(x, my, mx + lt - x, lt, c)
        r.fill(mx, y, lt, my + lt - y, c)


def draw_art(r, mark, x, y, cw, chh, lt, bright, dim):
    rows = len(mark.art)
    for j, line in enumerate(mark.art):
        c = bright
        if dim != bright and j >= rows // 2:
            c = mix_rgb(bright, dim, mark.art_lower)
        for i, ch in enumerate(line):
            draw_art_cell(r, x + i * cw, y + j * chh, ch, cw, chh, lt, c)


def draw_mark(r, mark, sc, lay):
    """The settled frame's mark as kdos-splash composes it in KDOS Modern:
    the wordmark flat, its lower half part of the way to the accent's dark,
    then the penguin. (The four-way phosphor bleed is Phosphor mode's.)"""
    cw, chh, lt = lay["art_cw"], lay["art_ch"], lay["art_lt"]
    draw_art(r, mark, lay["art_x"], lay["art_y"], cw, chh, lt, sc.window_focus, sc.window_hover)
    s = lay["peng_scale"]
    draw_penguin(r, mark, lay["peng_x"], lay["peng_y"], mark.w * s, mark.h * s, mascot_palette(sc, mark))


# ─── previews ─────────────────────────────────────────────────────────────


def render_preview(w, h, sc, mark):
    """A desktop drawn in the scheme's own roles: the mark on the ground, one
    window with a selected row, and the KDOS desk's bottom panel with its
    accent rule and running-task underline. It is generated, not captured,
    so it is exact for every accent and needs no running session."""
    r = Raster(w, h, sc.window_bg)
    u = h / 1080.0

    lay = splash_layout(w, h, mark)
    draw_mark(r, mark, sc, lay)

    # One window, outlined in the decoration's focus colour.
    wx, wy, ww, wh = int(w * 0.30), int(h * 0.49), int(w * 0.46), int(h * 0.39)
    title_h = max(3, int(30 * u))
    r.fill(wx - 1, wy - 1, ww + 2, wh + 2, sc.window_focus)
    r.fill(wx, wy, ww, title_h, sc.header_bg)
    r.fill(wx + int(14 * u), wy + title_h // 2 - max(1, int(4 * u)), int(ww * 0.28), max(2, int(8 * u)), sc.header_fg)
    bsz = max(2, int(12 * u))
    for i in range(3):
        bx = wx + ww - (i + 1) * (bsz + int(12 * u))
        r.fill(bx, wy + (title_h - bsz) // 2, bsz, bsz, sc.header_fg)
    body_y = wy + title_h
    r.fill(wx, body_y, ww, wh - title_h, sc.view_bg)
    side_w = int(ww * 0.24)
    r.fill(wx, body_y, side_w, wh - title_h, sc.window_alt)
    row_h = max(3, int(34 * u))
    for i in range(7):
        ry = body_y + int(12 * u) + i * row_h
        if ry + row_h > wy + wh:
            break
        if i == 2:
            r.fill(wx + side_w, ry, ww - side_w, row_h, sc.sel_bg)
            ink = sc.sel_fg
        else:
            if i % 2:
                r.fill(wx + side_w, ry, ww - side_w, row_h, sc.view_alt)
            ink = sc.view_fg
        r.fill(wx + side_w + int(18 * u), ry + row_h // 2 - max(1, int(4 * u)),
               int((ww - side_w) * (0.55 - 0.05 * (i % 3))), max(2, int(8 * u)), ink)
        r.fill(wx + int(16 * u), ry + row_h // 2 - max(1, int(3 * u)), int(side_w * 0.6), max(2, int(6 * u)), sc.window_inactive)

    # The KDOS desk: an opaque bottom panel with a one-pixel accent rule.
    ph = max(4, int(46 * u))
    py = h - ph
    r.fill(0, py, w, ph, sc.window_bg)
    r.fill(0, py, w, max(1, int(u)), sc.window_focus)
    isz = max(3, int(30 * u))
    iy = py + (ph - isz) // 2
    x = int(10 * u)
    draw_penguin(r, mark, x, iy, isz, isz, mascot_palette(sc, mark))
    x += isz + int(16 * u)
    for i in range(4):
        r.fill(x, iy + isz // 4, isz // 2, isz // 2, sc.window_focus if i == 0 else sc.window_inactive)
        x += isz // 2 + int(6 * u)
    x += int(14 * u)
    for i in range(4):
        r.fill(x, iy, isz, isz, sc.button_bg)
        r.fill(x + isz // 4, iy + isz // 4, isz // 2, isz // 2, sc.button_fg)
        if i == 1:
            r.fill(x - int(6 * u), py + ph - max(2, int(3 * u)), isz + int(12 * u), max(2, int(3 * u)), sc.window_focus)
        x += isz + int(18 * u)
    cx = w - int(150 * u)
    r.fill(cx, iy + int(2 * u), int(64 * u), max(2, int(10 * u)), sc.window_fg)
    r.fill(cx, iy + int(18 * u), int(92 * u), max(2, int(8 * u)), sc.window_inactive)
    for i in range(3):
        r.fill(cx - (i + 1) * int(34 * u), iy + isz // 3, isz // 2, isz // 2, sc.window_fg)
    return r


def render_splash(w, h, sc, mark):
    """The KSplash's settled frame at half its stages, laid out by the same
    arithmetic Splash.qml runs: the mark, the two lines under it (as bars of
    their width in glyph cells: this program sets no type), the rule and the
    segmented bar. The Splash Screen page shows it beside each global theme."""
    r = Raster(w, h, sc.window_bg)
    lay = splash_layout(w, h, mark)
    draw_mark(r, mark, sc, lay)

    s = lay["body_scale"]
    glyph = GLYPH_W * s
    unlit = spl_mix(sc.window_bg, sc.window_hover, 50)
    for text, y, ink in ((mark.sub, lay["sub_y"], sc.window_fg), (mark.tag, lay["tag_y"], sc.window_inactive)):
        tw = len(text) * glyph
        r.fill((w - tw) // 2, y + GLYPH_H * s // 4, tw, GLYPH_H * s // 2, ink)
    r.fill(lay["body_x"], lay["rule_y"], lay["body_w"], s, unlit)

    pct_w = 5 * glyph
    seg_x = lay["body_x"] + glyph + s * 2
    seg_area = lay["body_w"] - glyph * 2 - s * 4 - pct_w
    gap = s * 2
    seg_w = (seg_area - gap * (BAR_SEGS - 1)) // BAR_SEGS
    seg_h = GLYPH_H * s - s * 6
    if seg_w >= 2:
        lit = BAR_SEGS // 2
        for i in range(BAR_SEGS):
            c = sc.window_focus if i < lit else (sc.neutral if i == lit else unlit)
            r.fill(seg_x + i * (seg_w + gap), lay["bar_y"] + s * 3, seg_w, seg_h, c)
    return r


# ─── the global themes ────────────────────────────────────────────────────

AUTHOR = [{"Name": "KDOS"}]
WEBSITE = "https://github.com/kunaldawn/kdos"

# The fonts every accent applies. The UI face is Noto Sans, the fixed face
# JetBrains Mono (plan defaults B17, D7). KLookAndFeelManager APPLIES the
# [kdeglobals][General] keys but DETECTS the fonts flag from [kdeglobals][WM]:
# a package without a WM font key never has its fonts applied at all.
UI_FONT = "Noto Sans,10"
SMALL_FONT = "Noto Sans,8"
FIXED_FONT = "JetBrains Mono,10"


def lnf_defaults(sc, splash_id):
    icons = "KDOS-Light" if sc.light else "KDOS"
    sections = [
        ("kdeglobals][KDE", [("widgetStyle", "Breeze")]),
        ("kdeglobals][General", [
            ("ColorScheme", sc.scheme),
            ("font", UI_FONT),
            ("fixed", FIXED_FONT),
            ("smallestReadableFont", SMALL_FONT),
            ("toolBarFont", UI_FONT),
            ("menuFont", UI_FONT),
        ]),
        ("kdeglobals][WM", [("font", UI_FONT), ("activeFont", UI_FONT)]),
        ("kdeglobals][Icons", [("Theme", icons)]),
        ("plasmarc][Theme", [("name", "kdos")]),
        ("kcminputrc][Mouse", [("cursorTheme", "breeze_cursors")]),
        ("kwinrc][org.kde.kdecoration2", [
            ("library", "org.kde.breeze"),
            ("theme", "Breeze"),
        ]),
        ("kwinrc][WindowSwitcher", [("LayoutName", "thumbnail_grid")]),
        ("kwinrc][Windows", [("Placement", "Centered")]),
        ("ksplashrc][KSplash", [("Theme", splash_id)]),
        ("Wallpaper", [("Image", "KDOS-" + sc.title)]),
    ]
    out = []
    for group, keys in sections:
        out.append("[%s]" % group)
        out.extend("%s=%s" % kv for kv in keys)
        out.append("")
    return "\n".join(out)


def lnf_metadata(sc, pkg_id):
    tone = "light" if sc.light else "dark"
    meta = {
        "KPackageStructure": "Plasma/LookAndFeel",
        "KPlugin": {
            "Authors": AUTHOR,
            "Category": "",
            "Description": "KDOS Modern in the %s accent (%s)" % (sc.title, tone),
            "Id": pkg_id,
            "License": "MIT",
            "Name": "KDOS " + sc.title,
            "Version": "1.0",
            "Website": WEBSITE,
        },
        "Keywords": "Desktop;Workspace;Appearance;Look and Feel;KDOS;Accent;" + sc.title + ";",
        "X-Plasma-APIVersion": "2",
    }
    return json.dumps(meta, indent=4, sort_keys=True, ensure_ascii=False) + "\n"


def copy_tree(src, dst):
    for root, dirs, files in os.walk(src):
        dirs.sort()
        rel = os.path.relpath(root, src)
        for name in sorted(files):
            s = os.path.join(root, name)
            d = os.path.normpath(os.path.join(dst, rel, name))
            os.makedirs(os.path.dirname(d), exist_ok=True)
            shutil.copyfile(s, d)


# A global theme with one of these missing borrows Breeze's: the Breeze splash,
# the Breeze layout, a Breeze screenshot on the Global Theme or Splash Screen
# page.
LNF_FILES = (
    "metadata.json",
    "contents/defaults",
    "contents/splash/Splash.qml",
    "contents/splash/mark.js",
    "contents/layouts/org.kde.plasma.desktop-layout.js",
    "contents/layouts/defaults",
    "contents/plasmoidsetupscripts/org.kde.plasma.kickoff.js",
    "contents/previews/preview.png",
    "contents/previews/fullscreenpreview.jpg",
    "contents/previews/splash.png",
)


def write_lnf(args, sc, mark, scratch):
    pkg_id = "org.kdos.%s.desktop" % sc.accent
    root = os.path.join(args.pkg, "usr/share/plasma/look-and-feel", pkg_id)
    copy_tree(os.path.join(args.src, "lookandfeel"), root)
    write_text(os.path.join(root, "metadata.json"), lnf_metadata(sc, pkg_id))
    write_text(os.path.join(root, "contents/defaults"), lnf_defaults(sc, pkg_id))
    write_text(os.path.join(root, "contents/splash/mark.js"), mark.qml_js())
    write_bytes(os.path.join(root, "contents/previews/preview.png"),
                render_preview(1800, 1014, sc, mark).downscale(3).png())
    write_jpeg(os.path.join(root, "contents/previews/fullscreenpreview.jpg"),
               render_preview(1920, 1080, sc, mark), scratch)
    write_bytes(os.path.join(root, "contents/previews/splash.png"),
                render_splash(1800, 1014, sc, mark).downscale(3).png())
    for need in LNF_FILES:
        if not os.path.isfile(os.path.join(root, need)):
            die("%s: %s is missing; the Breeze package would stand in for it" % (pkg_id, need))


# ─── the Plasma style ─────────────────────────────────────────────────────

# Every class KSvg rewrites at load time (ImageSetPrivate::svgStyleSheet), with
# the scheme role it stands for. The colours written into the file are only
# placeholders that make it render in an editor; Plasma replaces the whole
# <style id="current-color-scheme"> block with the running scheme's.
def css_classes(sc):
    return [
        ("Text", sc.window_fg), ("Background", sc.window_bg),
        ("Highlight", sc.sel_bg), ("HighlightedText", sc.sel_fg),
        ("PositiveText", sc.positive), ("NeutralText", sc.neutral),
        ("NegativeText", sc.negative),
        ("ButtonText", sc.button_fg), ("ButtonBackground", sc.button_bg),
        ("ButtonHover", sc.button_hover), ("ButtonFocus", sc.button_focus),
        ("TooltipText", sc.tip_fg), ("TooltipBackground", sc.tip_bg),
    ]


class Svg:
    def __init__(self, sc):
        self.sc = sc
        self.parts = []
        self.w = self.h = 0

    def rect(self, ident, x, y, w, h, cls=None, opacity=1.0):
        self.w, self.h = max(self.w, x + w), max(self.h, y + h)
        attr = ' id="%s"' % ident if ident else ""
        if cls is None:
            style = "fill:none;stroke:none"
            klass = ""
        else:
            style = "fill:currentColor;fill-opacity:%s;stroke:none" % fmt(opacity)
            klass = ' class="ColorScheme-%s"' % cls
        return '<rect%s%s x="%s" y="%s" width="%s" height="%s" style="%s"/>' % (
            attr, klass, fmt(x), fmt(y), fmt(w), fmt(h), style)

    def add(self, s):
        self.parts.append(s)

    def group(self, ident, children):
        self.parts.append('<g id="%s">%s</g>' % (ident, "".join(children)))

    def text(self):
        style = "".join(".ColorScheme-%s{color:%s;}" % (n, hexc(c)) for n, c in css_classes(self.sc))
        return ('<?xml version="1.0" encoding="UTF-8"?>\n'
                '<svg xmlns="http://www.w3.org/2000/svg" version="1.1" width="%s" height="%s">\n'
                '<style id="current-color-scheme" type="text/css">%s</style>\n%s\n</svg>\n') % (
                    fmt(self.w), fmt(self.h), style, "\n".join(self.parts))


def fmt(v):
    return ("%d" % v) if float(v).is_integer() else ("%.3f" % v).rstrip("0")


SIDES = ("top", "bottom", "left", "right")


def frame(svg, prefix, ox, oy, border, layers, margins, insets=None, extra_hints=()):
    """A FrameSvg frame: nine elements laid out in a 3x3 block at (ox, oy)
    with `border`-pixel edges and a 16-pixel centre, then the hint elements.

    `layers` is a list of (cls, opacity, where): where is "fill" (every
    element, the whole frame tinted), "center" (the centre only), or a side
    name and a thickness, ("line", side, px), drawn along that edge. Square by
    construction: a corner is a full square, never an arc."""
    p = (prefix + "-") if prefix else ""
    c = 16
    b = border
    cells = {
        "topleft": (0, 0, b, b), "top": (b, 0, c, b), "topright": (b + c, 0, b, b),
        "left": (0, b, b, c), "center": (b, b, c, c), "right": (b + c, b, b, c),
        "bottomleft": (0, b + c, b, b), "bottom": (b, b + c, c, b), "bottomright": (b + c, b + c, b, b),
    }
    for name, (x, y, w, h) in cells.items():
        x, y = ox + x, oy + y
        kids = [svg.rect(None, x, y, w, h)]
        for layer in layers:
            cls, op, where = layer[0], layer[1], layer[2]
            if where == "fill" or (where == "center" and name == "center"):
                kids.append(svg.rect(None, x, y, w, h, cls, op))
            elif where == "line":
                side, px = layer[3], layer[4]
                if side not in name:
                    continue
                if side == "top":
                    kids.append(svg.rect(None, x, y, w, px, cls, op))
                elif side == "bottom":
                    kids.append(svg.rect(None, x, y + h - px, w, px, cls, op))
                elif side == "left":
                    kids.append(svg.rect(None, x, y, px, h, cls, op))
                else:
                    kids.append(svg.rect(None, x + w - px, y, px, h, cls, op))
        svg.group(p + name, kids)
    hy = oy + b * 2 + c + 4
    hx = ox
    for side in SIDES:
        m = margins[side]
        w, h = (1, m) if side in ("top", "bottom") else (m, 1)
        svg.add(svg.rect(p + "hint-%s-margin" % side, hx, hy, w, h))
        hx += m + 2
    if insets:
        for side in SIDES:
            m = insets[side]
            w, h = (1, m) if side in ("top", "bottom") else (m, 1)
            svg.add(svg.rect(p + "hint-%s-inset" % side, hx, hy, w, h))
            hx += m + 2
    for name in extra_hints:
        svg.add(svg.rect(p + name, hx, hy, 1, 1))
        hx += 3
    return oy + b * 2 + c + 4 + max(max(margins.values()), max((insets or margins).values())) + 6


def uniform(m):
    return {s: m for s in SIDES}


# The accent in the Plasma style's SVGs. KSvg's Highlight class is the
# scheme's selection, which on a dark accent is the accent darkened until the
# view's text reads on it; a rule or an outline drawn in it would be that
# muddier colour. ButtonFocus is the Button set's DecorationFocus, the accent
# itself in every KDOS scheme. Translucent fills under text stay Highlight,
# the colour text is solved to read on.
ACCENT = "ButtonFocus"


def theme_plate(sc, bg_cls, rule_cls, rule_op):
    """Panel, popup and tooltip: one opaque plate with a one-pixel rule on
    every edge. FrameSvg disables the edges a panel sits against, so a bottom
    panel shows only its top rule and a floating one is outlined."""
    svg = Svg(sc)
    layers = [(bg_cls, 1.0, "fill")] + [(rule_cls, rule_op, "line", s, 1) for s in SIDES]
    y = frame(svg, "", 0, 0, 1, layers, uniform(4), insets=uniform(4))
    return svg, y


def theme_panel(sc):
    svg, y = theme_plate(sc, "Background", ACCENT, 1.0)
    # Panel.qml pads applets with the `thick` margins; Breeze's values, so a
    # KDOS panel lays its applets out exactly where a Breeze one does.
    margins = {"top": 8, "bottom": 8, "left": 4, "right": 4}
    for i, side in enumerate(SIDES):
        m = margins[side]
        w, h = (1, m) if side in ("top", "bottom") else (m, 1)
        svg.add(svg.rect("thick-hint-%s-margin" % side, 60 + i * 12, y, w, h))
    svg.add(svg.rect("thick-center", 120, y, 16, 16))
    return svg.text()


def theme_dialog(sc):
    return theme_plate(sc, "Background", ACCENT, 1.0)[0].text()


def theme_tooltip(sc):
    return theme_plate(sc, "TooltipBackground", "TooltipText", 0.35)[0].text()


def theme_button(sc):
    svg = Svg(sc)
    y = 0
    outline = [("line", s, 1) for s in SIDES]
    m = uniform(4)
    specs = [
        ("normal", [("ButtonBackground", 1.0, "fill")] + [("ButtonText", 0.3) + o for o in outline]),
        ("pressed", [("ButtonBackground", 1.0, "fill"), ("Highlight", 0.3, "fill")]
         + [(ACCENT, 1.0) + o for o in outline]),
        ("hover", [("ButtonHover", 1.0) + o for o in outline]),
        ("focus", [("ButtonFocus", 1.0) + o for o in outline]),
        ("toolbutton-hover", [("Highlight", 0.14, "fill")] + [(ACCENT, 0.6) + o for o in outline]),
        ("toolbutton-pressed", [("Highlight", 0.3, "fill")] + [(ACCENT, 1.0) + o for o in outline]),
        ("toolbutton-focus", [("ButtonFocus", 1.0) + o for o in outline]),
    ]
    for prefix, layers in specs:
        y = frame(svg, prefix, 0, y, 1, layers, m)
    return svg.text()


# The Task Manager's running indicator is an accent underline on the side the
# panel sits against; the north/west/east sets move it to that edge and the
# unprefixed set is the bottom panel's (TaskTools.taskPrefix falls back to it).
TASK_EDGE = {"": "bottom", "north": "top", "west": "left", "east": "right"}


def theme_tasks(sc):
    svg = Svg(sc)
    y = 0
    m = uniform(4)
    for loc, edge in TASK_EDGE.items():
        def p(name):
            return (loc + "-" + name) if loc else name
        states = [
            ("normal", [(ACCENT, 0.6, "line", edge, 2)]),
            ("focus", [("Highlight", 0.16, "fill"), (ACCENT, 1.0, "line", edge, 3)]),
            ("hover", [("Highlight", 0.1, "fill"), ("ButtonHover", 1.0, "line", edge, 2)]),
            ("attention", [("NeutralText", 0.16, "fill"), ("NeutralText", 1.0, "line", edge, 3)]),
            ("minimized", [("Text", 0.35, "line", edge, 2)]),
            ("progress", [("PositiveText", 0.28, "fill")]),
        ]
        for state, layers in states:
            y = frame(svg, p(state), 0, y, 4, layers, m)
    # The group arrow sits on the indicator's edge and points away from it; the
    # invisible rect keeps the arrow clear of a three-pixel underline.
    x = 0
    for edge in ("bottom", "top", "left", "right"):
        if edge in ("bottom", "top"):
            box = (x, y, 8, 9)
            if edge == "bottom":
                tri = "M%s,%s L%s,%s L%s,%s Z" % (x, y + 4, x + 4, y, x + 8, y + 4)
            else:
                tri = "M%s,%s L%s,%s L%s,%s Z" % (x, y + 5, x + 4, y + 9, x + 8, y + 5)
        else:
            box = (x, y, 9, 8)
            if edge == "left":
                tri = "M%s,%s L%s,%s L%s,%s Z" % (x + 5, y, x + 9, y + 4, x + 5, y + 8)
            else:
                tri = "M%s,%s L%s,%s L%s,%s Z" % (x + 4, y, x, y + 4, x + 4, y + 8)
        svg.w, svg.h = max(svg.w, box[0] + box[2]), max(svg.h, box[1] + box[3])
        svg.group("group-expander-" + edge, [
            svg.rect(None, *box),
            '<path class="ColorScheme-Text" d="%s" style="fill:currentColor;fill-opacity:0.85;stroke:none"/>' % tri,
        ])
        x += 14
    return svg.text()


# The pager's desktop cells keep the panel's own ground and carry no fill:
# the desktop's label is drawn in the plain text colour whatever the state,
# so a filled cell would put light text on a light accent. The current
# desktop is an accent outline over a two-pixel accent rule along its bottom,
# a hovered one a hover-coloured outline, and the rest a faint outline that
# shows where each cell ends.
def theme_pager(sc):
    svg = Svg(sc)
    y = 0
    outline = [("line", s, 1) for s in SIDES]
    m = uniform(2)
    specs = [
        ("normal", [("Text", 0.25) + o for o in outline]),
        ("hover", [("ButtonHover", 1.0) + o for o in outline]),
        ("active", [(ACCENT, 1.0) + o for o in outline] + [(ACCENT, 1.0, "line", "bottom", 2)]),
    ]
    for prefix, layers in specs:
        y = frame(svg, prefix, 0, y, 2, layers, m)
    return svg.text()


def svgz(path, text):
    """gzip with no name and no timestamp, so the bytes depend on the text."""
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as raw:
        with gzip.GzipFile(filename="", mode="wb", fileobj=raw, compresslevel=9, mtime=0) as z:
            z.write(text.encode("utf-8"))


def write_desktoptheme(args, sc):
    root = os.path.join(args.pkg, "usr/share/plasma/desktoptheme/kdos")
    copy_tree(os.path.join(args.src, "desktoptheme"), root)
    panel, dialog, tooltip = theme_panel(sc), theme_dialog(sc), theme_tooltip(sc)
    # The solid/ copies are what an opaque panel, a solid dialog and every
    # PlasmaComponents tooltip and menu ask for by path. Without them KSvg
    # falls through to the Breeze theme's rounded ones.
    for sub in ("", "solid/"):
        svgz(os.path.join(root, sub + "widgets/panel-background.svgz"), panel)
        svgz(os.path.join(root, sub + "dialogs/background.svgz"), dialog)
        svgz(os.path.join(root, sub + "widgets/tooltip.svgz"), tooltip)
    svgz(os.path.join(root, "widgets/button.svgz"), theme_button(sc))
    svgz(os.path.join(root, "widgets/tasks.svgz"), theme_tasks(sc))
    svgz(os.path.join(root, "widgets/pager.svgz"), theme_pager(sc))


# ─── the desk templates ───────────────────────────────────────────────────


def write_desks(args):
    src = os.path.join(args.src, "desks")
    for name in sorted(os.listdir(src)):
        if not name.startswith("org.kdos.desk."):
            continue
        meta = os.path.join(src, name, "metadata.json")
        with open(meta, encoding="utf-8") as f:
            data = json.load(f)
        # loadTemplate() finds a template only through this category, and
        # Add Panel lists every template that has it unless NoDisplay is set.
        if "panel" not in data.get("X-Plasma-ContainmentCategories", []) or data.get("NoDisplay") is not True:
            die("%s: a desk must be a panel template with NoDisplay" % meta)
        if data.get("KPlugin", {}).get("Id") != name:
            die("%s: KPlugin Id is not %s" % (meta, name))
        copy_tree(os.path.join(src, name), os.path.join(args.pkg, "usr/share/plasma/layout-templates", name))


def main():
    ap = argparse.ArgumentParser(description="KDOS global themes, Plasma style and previews")
    ap.add_argument("--src", required=True)
    ap.add_argument("--pkg", required=True)
    ap.add_argument("--schemes", required=True)
    ap.add_argument("--splash", required=True)
    ap.add_argument("--accents", required=True, nargs="+")
    args = ap.parse_args()

    schemes = load_schemes(args.schemes, args.accents)
    mark = Mark(args.splash)
    scratch = os.path.join(os.getcwd(), "genlook.tmp")
    os.makedirs(scratch, exist_ok=True)

    for sc in schemes:
        write_lnf(args, sc, mark, scratch)
    write_desktoptheme(args, schemes[0])
    write_desks(args)
    shutil.rmtree(scratch)


if __name__ == "__main__":
    main()
