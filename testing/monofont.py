#!/usr/bin/env python3
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   monofont — what the KDOS monospace face covers, and the cell it makes
# ---------------------------------
#
#   python3 testing/monofont.py [--root build/fs] [--family "JetBrains Mono"]
#                               [--fallback "Noto Sans Mono"]... [--json]
#   python3 testing/monofont.py --face FILE [--fallback-face FILE]...
#   python3 testing/monofont.py --selftest
#
# Needs fontTools (the python3-fonttools port; on a development host any
# fontTools 4). A face is found by family and the style a plain request gets,
# the upright one nearest Regular: in a built tree's usr/share/fonts with
# --root, through fc-list otherwise; --face names the file outright.
#
# The fallbacks are the families the shipped fontconfig file
# (etc/fonts/conf.d/*kdos-mono.conf: the tree's with --root, fs/'s otherwise)
# prefers for `monospace` after the face, in its order, so the chain measured
# is the chain that ships. --fallback or --fallback-face replaces it. A
# shipped file whose `monospace` list does not put the face first fails:
# every program asking for `monospace` would get another face.
#
# The face is what Konsole, Kate and every `monospace` request draws with, so
# it is gated on what a terminal shows:
#
#   coverage   box drawing U+2500-257F, block elements U+2580-259F, braille
#              U+2800-28FF (btop and every braille plot), Latin Extended-A
#              and -B, Greek and Cyrillic: every assigned code point is in the
#              face or in a fallback face. A hole in the face that a fallback
#              fills passes and is reported, because fontconfig fills it the
#              same way. A hole nothing in the chain fills fails.
#   the cell   measured on 'M', never the face's maximum advance: a monospace
#              face that carries one wide glyph reports a maximum twice its
#              cell, and a terminal sized from that draws every character in
#              a gap. Every covered glyph of the gated ranges must advance by
#              exactly the cell, or it overflows into the next one.
#   0 and O    the zero is marked: it has more contours than O (a dot or a
#              slash inside it).
#   1, l, I    three different outlines, each pair differing in at least
#              DISTINCT of the cell when drawn into one grid.
#
# Recorded with no gate: the cell and line height in pixels at 10 pt and at
# scale 1, 1.25 and 2, the x-height, the hinting tables, the ligature
# features (the KDOS Konsole profile turns ligatures off) and Powerline
# coverage.
#
# --selftest builds, with fontTools, a face that passes and one that fails each
# gate, and checks every verdict: the proof the gates can fail.
#
# Exit 0 the face passes, 1 a gate fails or the shipped chain leaves the face
# out, 2 no fontTools, no such face or no shipped chain.

import argparse
import glob
import json
import os
import shutil
import subprocess
import sys
import unicodedata

try:
    from fontTools.ttLib import TTFont
    from fontTools.pens.recordingPen import RecordingPen
    from fontTools.pens.boundsPen import BoundsPen
    from fontTools.pens.basePen import BasePen
except ImportError:
    TTFont = None
    BasePen = object

# (name, first, last, code points left out). Greek is its block without the
# Coptic letters U+03E2-03EF, which no Greek text uses.
BLOCKS = [
    ("Box Drawing", 0x2500, 0x257F, ()),
    ("Block Elements", 0x2580, 0x259F, ()),
    ("Braille Patterns", 0x2800, 0x28FF, ()),
    ("Latin Extended-A", 0x0100, 0x017F, ()),
    ("Latin Extended-B", 0x0180, 0x024F, ()),
    ("Greek", 0x0370, 0x03FF, tuple(range(0x03E2, 0x03EF + 1))),
    ("Cyrillic", 0x0400, 0x04FF, ()),
]
# 1, l and I must differ in at least this share of the cell's pixels when
# both are drawn into one grid; two glyphs that differ in less read as one.
DISTINCT = 0.03
GRID = (32, 64)
POWERLINE = list(range(0xE0A0, 0xE0A3 + 1)) + list(range(0xE0B0, 0xE0B3 + 1))
# The fallback chain is the one the shipped fontconfig file names: the
# families of its `monospace` <prefer> after the gated face, in order. It is
# read from that file rather than written here, so a change to the chain is
# measured the day it is made, never against a list this script remembers.
# Konsole draws box drawing, block elements and braille itself unless a
# profile asks for the font's, so the chain is what every other program shows.
MONO_CONF = "etc/fonts/conf.d/*kdos-mono.conf"
POINT_SIZE = 10
SCALES = (1.0, 1.25, 2.0)


def assigned(cp):
    return unicodedata.category(chr(cp)) not in ("Cn", "Co", "Cs")


def zero_width(cp):
    return unicodedata.category(chr(cp)) in ("Mn", "Me", "Cf")


def names(font):
    def get(*ids):
        for i in ids:
            rec = font["name"].getName(i, 3, 1, 0x409) or font["name"].getName(i, 1, 0, 0)
            if rec:
                return str(rec)
        return ""
    return get(16, 1), get(17, 2), get(5)


def plain_rank(font):
    """How far a face is from what a plain request gets: None for an italic,
    else the distance of its weight from 400. fontconfig hands `family` alone
    the upright face nearest Regular, which for Terminus (TTF) is its Medium
    (500): a lookup that took only weight 400 would find no face there."""
    os2 = font["OS/2"] if "OS/2" in font else None
    weight = os2.usWeightClass if os2 else 400
    if os2 and os2.fsSelection & 1:
        return None
    return (abs(weight - 400), weight)


def squash(text):
    return "".join(ch for ch in text.lower() if ch.isalnum())


def font_files(root):
    base = os.path.join(root, "usr/share/fonts") if root else None
    pats = ("**/*.ttf", "**/*.otf", "**/*.TTF", "**/*.OTF")
    if base:
        return [p for pat in pats for p in glob.glob(os.path.join(base, pat), recursive=True)]
    return []


def best_face(family, paths):
    best = None
    for path in sorted(paths):
        try:
            f = TTFont(path, lazy=True, fontNumber=0)
            fam, _style, _v = names(f)
            rank = plain_rank(f)
        except Exception:
            continue
        if fam == family and rank is not None and (best is None or rank < best[0]):
            best = (rank, path)
    return best[1] if best else None


def find_face(family, root):
    """The file of `family` a plain request draws with: the built tree's font
    directory with --root, fc-list's answer otherwise."""
    if root:
        key = squash(family)
        return best_face(family, [p for p in font_files(root) if key in squash(os.path.basename(p))])
    if not shutil.which("fc-list"):
        return None
    # By family alone: a face's Regular may be named Book (DejaVu), and the
    # weight and slant decide what fontconfig would hand a plain request.
    out = subprocess.run(["fc-list", "--format", "%{file}\n", family],
                         capture_output=True, text=True).stdout.split("\n")
    return best_face(family, [p for p in out if p])


def shipped_chain(root):
    """The shipped `monospace` <prefer> list and the file it is in: the built
    tree's with --root, the checkout's fs/ otherwise. ([], None) when no file
    ships one."""
    import xml.etree.ElementTree as ET
    for conf in sorted(glob.glob(os.path.join(root or "fs", MONO_CONF))):
        try:
            tree = ET.parse(conf)
        except (OSError, ET.ParseError):
            continue
        for alias in tree.getroot().iter("alias"):
            fam = alias.find("family")
            pref = alias.find("prefer")
            if fam is None or pref is None or (fam.text or "").strip() != "monospace":
                continue
            chain = [(x.text or "").strip() for x in pref.findall("family")]
            return [x for x in chain if x], conf
    return [], None


def chain_after(prefer, family):
    """The fallbacks after `family` in a shipped `monospace` list, or None
    when the list does not put it first: the first face in that list draws
    every glyph it has, so a face second in it is a fallback, not the face."""
    if not prefer or prefer[0] != family:
        return None
    return prefer[1:]


class FlattenPen(BasePen):
    """A glyph's outline as closed polygons, every curve cut into eight."""

    def __init__(self, glyphset):
        super().__init__(glyphset)
        self.polys, self.cur = [], []

    def _moveTo(self, p):
        self.cur = [p]

    def _lineTo(self, p):
        self.cur.append(p)

    def _curveToOne(self, p1, p2, p3):
        p0 = self.cur[-1]
        for i in range(1, 9):
            t = i / 8.0
            u = 1 - t
            self.cur.append((u ** 3 * p0[0] + 3 * u * u * t * p1[0] + 3 * u * t * t * p2[0] + t ** 3 * p3[0],
                             u ** 3 * p0[1] + 3 * u * u * t * p1[1] + 3 * u * t * t * p2[1] + t ** 3 * p3[1]))

    def _qCurveToOne(self, p1, p2):
        p0 = self.cur[-1]
        for i in range(1, 9):
            t = i / 8.0
            u = 1 - t
            self.cur.append((u * u * p0[0] + 2 * u * t * p1[0] + t * t * p2[0],
                             u * u * p0[1] + 2 * u * t * p1[1] + t * t * p2[1]))

    def _closePath(self):
        if len(self.cur) > 2:
            self.polys.append(self.cur)
        self.cur = []

    _endPath = _closePath


def raster(glyphset, name, cell, lo, hi):
    """The glyph's coverage of a GRID over its cell, nonzero winding at each
    sample's centre: a set of filled (column, row)."""
    pen = FlattenPen(glyphset)
    glyphset[name].draw(pen)
    cols, rows = GRID
    filled = set()
    for r in range(rows):
        y = lo + (r + 0.5) * (hi - lo) / rows
        for c in range(cols):
            x = (c + 0.5) * cell / cols
            wind = 0
            for poly in pen.polys:
                for (x0, y0), (x1, y1) in zip(poly, poly[1:] + poly[:1]):
                    if (y0 <= y) != (y1 <= y):
                        xi = x0 + (y - y0) * (x1 - x0) / (y1 - y0)
                        if xi > x:
                            wind += 1 if y1 > y0 else -1
            if wind:
                filled.add((c, r))
    return filled


def signature(glyphset, name):
    pen = RecordingPen()
    glyphset[name].draw(pen)
    rec = tuple((op, tuple(tuple(round(c) for c in pt) for pt in args)) for op, args in pen.value)
    contours = sum(1 for op, _ in pen.value if op in ("closePath", "endPath"))
    bp = BoundsPen(glyphset)
    glyphset[name].draw(bp)
    return rec, contours, bp.bounds


class Face:
    def __init__(self, path):
        self.path = path
        self.font = TTFont(path, fontNumber=0)
        self.cmap = self.font.getBestCmap() or {}
        self.hmtx = self.font["hmtx"].metrics
        self.upem = self.font["head"].unitsPerEm
        self.family, self.style, self.version = names(self.font)

    def advance(self, cp):
        g = self.cmap.get(cp)
        return self.hmtx[g][0] if g in self.hmtx else None


def report(primary, fallbacks):
    f = primary
    font = f.font
    r = {"face": {"file": f.path, "family": f.family, "style": f.style, "version": f.version},
         "fallbacks": [{"file": x.path, "family": x.family, "version": x.version} for x in fallbacks],
         "fail": []}

    cell = f.advance(ord("M"))
    if not cell:
        r["fail"].append("the face has no 'M', so it has no cell to measure")
        return r
    hhea = font["hhea"]
    os2 = font["OS/2"] if "OS/2" in font else None
    line = hhea.ascent - hhea.descent + hhea.lineGap
    px = POINT_SIZE * 96.0 / 72.0
    r["metrics"] = {
        "units_per_em": f.upem,
        "cell_advance_M": cell,
        "max_advance": hhea.advanceWidthMax,
        "max_advance_over_cell": round(hhea.advanceWidthMax / cell, 3),
        "post_isFixedPitch": bool(font["post"].isFixedPitch),
        "x_height": getattr(os2, "sxHeight", None) if os2 else None,
        "x_height_ratio": round(os2.sxHeight / f.upem, 3) if os2 and getattr(os2, "sxHeight", 0) else None,
        "cap_height": getattr(os2, "sCapHeight", None) if os2 else None,
        "hhea_ascent": hhea.ascent, "hhea_descent": hhea.descent, "hhea_line_gap": hhea.lineGap,
        "cell_px": {str(s): round(cell / f.upem * px * s, 2) for s in SCALES},
        "line_px": {str(s): round(line / f.upem * px * s, 2) for s in SCALES},
        "hinted": "fpgm" in font or "prep" in font,
    }
    gsub = font["GSUB"].table if "GSUB" in font else None
    feats = sorted({fr.FeatureTag for fr in gsub.FeatureList.FeatureRecord}) if gsub and gsub.FeatureList else []
    r["metrics"]["ligature_features"] = [t for t in feats if t in ("calt", "liga", "dlig", "rlig")]
    r["metrics"]["powerline"] = "%d of %d" % (sum(1 for cp in POWERLINE if cp in f.cmap), len(POWERLINE))

    blocks = []
    holes_total = 0
    for name, lo, hi, skip in BLOCKS:
        cps = [cp for cp in range(lo, hi + 1) if assigned(cp) and cp not in skip]
        mine = [cp for cp in cps if cp in f.cmap]
        filled = {}
        holes = []
        wide = 0
        for cp in cps:
            if cp in f.cmap:
                continue
            by = next((x for x in fallbacks if cp in x.cmap), None)
            if by:
                filled.setdefault(by.family, []).append(cp)
                # Reported, not gated: a fallback's glyph wider than the
                # fallback's own cell is wider than the face's too.
                own = by.advance(ord("M"))
                if own and by.advance(cp) and by.advance(cp) > own and not zero_width(cp):
                    wide += 1
            else:
                holes.append(cp)
        offcell = [cp for cp in mine if f.advance(cp) != cell and not (zero_width(cp) and f.advance(cp) == 0)]
        blocks.append({"block": name, "range": "U+%04X-%04X" % (lo, hi), "assigned": len(cps),
                       "in_face": len(mine),
                       "filled_by": {k: len(v) for k, v in filled.items()},
                       "holes": ["U+%04X" % c for c in holes],
                       "off_cell": ["U+%04X" % c for c in offcell],
                       "fallback_wider_than_its_cell": wide})
        holes_total += len(holes)
        if holes:
            r["fail"].append("%s: %d assigned code points in no face (%s%s)" %
                             (name, len(holes), ", ".join("U+%04X" % c for c in holes[:6]),
                              "…" if len(holes) > 6 else ""))
        if offcell:
            r["fail"].append("%s: %d glyphs do not advance by the cell of %d units (%s%s)" %
                             (name, len(offcell), cell, ", ".join("U+%04X" % c for c in offcell[:6]),
                              "…" if len(offcell) > 6 else ""))
    r["coverage"] = blocks

    gs = font.getGlyphSet()
    shapes = {}
    for ch in "0O1lI":
        g = f.cmap.get(ord(ch))
        if g is None:
            r["fail"].append("the face has no %r" % ch)
            continue
        rec, contours, bounds = signature(gs, g)
        shapes[ch] = (rec, contours, bounds)
    r["shapes"] = {ch: {"contours": s[1], "bounds": list(s[2]) if s[2] else None} for ch, s in shapes.items()}
    if "0" in shapes and "O" in shapes:
        if shapes["0"][1] <= shapes["O"][1]:
            r["fail"].append("the zero is unmarked: 0 has %d contours and O has %d" %
                             (shapes["0"][1], shapes["O"][1]))
    lo, hi = hhea.descent, hhea.ascent
    grids = {ch: raster(gs, f.cmap[ord(ch)], cell, lo, hi) for ch in shapes}
    area = GRID[0] * GRID[1]
    r["distinct"] = {}
    for a, b in (("0", "O"), ("1", "l"), ("1", "I"), ("l", "I")):
        if a not in shapes or b not in shapes:
            continue
        share = len(grids[a] ^ grids[b]) / float(area)
        r["distinct"]["%s/%s" % (a, b)] = round(share, 4)
        if a == "0":
            continue
        if shapes[a][0] == shapes[b][0]:
            r["fail"].append("%s and %s are the same outline" % (a, b))
        elif share < DISTINCT:
            r["fail"].append("%s and %s differ in %.1f%% of the cell, under %.0f%%" %
                             (a, b, share * 100, DISTINCT * 100))
    return r


def print_report(r):
    face = r["face"]
    print("monofont: %s %s (%s)" % (face["family"], face["style"], face["version"]))
    print("  file          %s" % face["file"])
    for fb in r["fallbacks"]:
        print("  fallback      %s (%s) %s" % (fb["family"], fb["version"], fb["file"]))
    m = r.get("metrics")
    if m:
        print("  cell          %d of %d units on 'M'; maximum advance %d (%.3gx the cell)%s" %
              (m["cell_advance_M"], m["units_per_em"], m["max_advance"], m["max_advance_over_cell"],
               "" if m["post_isFixedPitch"] else "; post.isFixedPitch is 0"))
        print("  at %d pt      cell %s px, line %s px at scale %s" %
              (POINT_SIZE, " / ".join(str(m["cell_px"][str(s)]) for s in SCALES),
               " / ".join(str(m["line_px"][str(s)]) for s in SCALES),
               " / ".join(str(s) for s in SCALES)))
        if m["x_height_ratio"]:
            print("  x-height      %d units, %.3g of the em" % (m["x_height"], m["x_height_ratio"]))
        print("  hinted        %s" % ("yes (fpgm/prep)" if m["hinted"] else "no"))
        print("  ligatures     %s" % (", ".join(m["ligature_features"]) or "none"))
        print("  powerline     %s" % m["powerline"])
    for b in r.get("coverage", []):
        fill = "; ".join("%d from %s" % (n, fam) for fam, n in b["filled_by"].items())
        print("  %-18s %3d/%-3d in the face%s%s%s%s" % (
            b["block"], b["in_face"], b["assigned"], "; " + fill if fill else "",
            "; %d in no face" % len(b["holes"]) if b["holes"] else "",
            "; %d off the cell" % len(b["off_cell"]) if b["off_cell"] else "",
            "; %d fallback glyphs wider than a cell" % b["fallback_wider_than_its_cell"]
            if b["fallback_wider_than_its_cell"] else ""))
    for ch, s in r.get("shapes", {}).items():
        print("  glyph %s       %d contours, box %s" % (ch, s["contours"], s["bounds"]))
    for pair, share in r.get("distinct", {}).items():
        print("  %-13s differ in %.1f%% of the cell" % (pair, share * 100))
    for fmsg in r["fail"]:
        print("  FAIL  %s" % fmsg)
    print("monofont: %s" % ("passes" if not r["fail"] else "%d gate(s) fail" % len(r["fail"])))


def fixture_face(path, family, change=None):
    """A synthetic face that passes every gate, or fails exactly the one
    `change` names: 'zero' (an unmarked zero), 'oneell' (1 and l one
    outline), 'wide' (a box-drawing glyph wider than the cell), 'hole' (no
    U+2500)."""
    from fontTools.fontBuilder import FontBuilder
    from fontTools.pens.ttGlyphPen import TTGlyphPen

    def rects(*boxes):
        pen = TTGlyphPen(None)
        for i, (x0, y0, x1, y1) in enumerate(boxes):
            # Outer contours clockwise, holes counter-clockwise.
            pts = [(x0, y0), (x0, y1), (x1, y1), (x1, y0)]
            if i and boxes[0][0] < x0 and x1 < boxes[0][2] and y1 - y0 > 150:
                pts.reverse()
            pen.moveTo(pts[0])
            for pt in pts[1:]:
                pen.lineTo(pt)
            pen.closePath()
        return pen.glyph()

    glyphs = {
        ".notdef": rects((50, 0, 550, 700)),
        "M": rects((60, 0, 540, 700)),
        "cell": rects((0, -200, 600, 800)),
        "zero": rects((80, 0, 520, 700), (160, 80, 440, 620), (260, 300, 340, 400)),
        "O": rects((80, 0, 520, 700), (160, 80, 440, 620)),
        "one": rects((260, 0, 340, 700), (150, 0, 450, 60), (120, 560, 260, 660)),
        "l": rects((260, 0, 340, 700), (340, 0, 540, 80)),
        "I": rects((270, 0, 330, 700), (120, 0, 480, 80), (120, 620, 480, 700)),
        "wide": rects((0, -200, 1200, 800)),
    }
    if change == "zero":
        glyphs["zero"] = rects((90, 0, 510, 700), (170, 80, 430, 620))
    if change == "oneell":
        glyphs["l"] = glyphs["one"]
    cmap = {ord("M"): "M", ord("0"): "zero", ord("O"): "O", ord("1"): "one", ord("l"): "l",
            ord("I"): "I"}
    for _name, lo, hi, skip in BLOCKS:
        for cp in range(lo, hi + 1):
            if assigned(cp) and cp not in skip:
                cmap[cp] = "cell"
    if change == "wide":
        cmap[0x2502] = "wide"
    if change == "hole":
        del cmap[0x2500]
    order = list(glyphs)
    fb = FontBuilder(1000, isTTF=True)
    fb.setupGlyphOrder(order)
    fb.setupCharacterMap(cmap)
    fb.setupGlyf(glyphs)
    fb.setupHorizontalMetrics({g: (1200 if g == "wide" else 600, 0) for g in order})
    fb.setupHorizontalHeader(ascent=800, descent=-200)
    fb.setupNameTable({"familyName": family, "styleName": "Regular"})
    fb.setupOS2(sTypoAscender=800, sTypoDescender=-200, usWinAscent=800, usWinDescent=200,
                usWeightClass=400, fsSelection=0x40, sxHeight=500)
    fb.setupPost(isFixedPitch=1)
    fb.save(path)


def selftest(tmp):
    """Every gate fails on the face built to fail it, and passes the face
    built to pass, or the gate is decoration."""
    want = {None: [], "zero": ["the zero is unmarked"], "oneell": ["1 and l are the same outline"],
            "wide": ["Box Drawing: 1 glyphs do not advance"], "hole": ["Box Drawing: 1 assigned code points in no face"]}
    bad = 0
    for change, expect in want.items():
        p = os.path.join(tmp, "fixture-%s.ttf" % (change or "good"))
        fixture_face(p, "Fixture Mono", change)
        r = report(Face(p), [])
        got = r["fail"]
        ok = (not got) if not expect else (len(got) == len(expect) and
                                           all(any(g.startswith(e) for g in got) for e in expect))
        print("  %s  %-6s %s" % ("ok  " if ok else "FAIL", change or "good",
                                 "passes" if not got else "; ".join(got)))
        bad += not ok
    # A hole the face has and a fallback fills passes.
    holed = os.path.join(tmp, "fixture-hole.ttf")
    full = os.path.join(tmp, "fixture-good.ttf")
    r = report(Face(holed), [Face(full)])
    ok = not r["fail"]
    print("  %s  hole filled by a fallback %s" % ("ok  " if ok else "FAIL", "passes" if ok else r["fail"]))
    bad += not ok

    # The chain is the shipped file's `monospace` list, in its order, and an
    # alias for another family is not it.
    conf = os.path.join(tmp, "etc/fonts/conf.d")
    os.makedirs(conf, exist_ok=True)
    with open(os.path.join(conf, "45-kdos-mono.conf"), "w") as f:
        f.write('<?xml version="1.0"?>\n<fontconfig>\n'
                '  <alias><family>Fixture Mono</family><default><family>monospace</family></default></alias>\n'
                '  <alias><family>monospace</family><prefer>\n'
                '    <family>Fixture Mono</family><family>Fill One</family><family>Fill Two</family>\n'
                '  </prefer></alias>\n</fontconfig>\n')
    chain, _ = shipped_chain(tmp)
    ok = chain == ["Fixture Mono", "Fill One", "Fill Two"]
    print("  %s  the shipped monospace chain reads in order %s" % ("ok  " if ok else "FAIL", chain))
    bad += not ok
    ok = chain_after(chain, "Fixture Mono") == ["Fill One", "Fill Two"] and \
        chain_after(chain, "Fill One") is None and chain_after([], "Fixture Mono") is None
    print("  %s  a face is gated only when the chain puts it first" % ("ok  " if ok else "FAIL"))
    bad += not ok
    return 1 if bad else 0


def main():
    ap = argparse.ArgumentParser(description="the KDOS monospace face's coverage and cell")
    ap.add_argument("--root", help="a built tree; faces are looked for in its usr/share/fonts")
    ap.add_argument("--family", default="JetBrains Mono", help="the face to gate (default JetBrains Mono)")
    ap.add_argument("--fallback", action="append", help="a family fontconfig falls back to (repeatable;"
                    " default: the shipped %s's monospace chain after the face)" % MONO_CONF)
    ap.add_argument("--face", help="the face's file, in place of --family")
    ap.add_argument("--fallback-face", action="append", default=[], help="a fallback's file (repeatable)")
    ap.add_argument("--json", action="store_true", help="the report as JSON")
    ap.add_argument("--selftest", action="store_true",
                    help="build a face to fail each gate and one to pass, and check the verdicts")
    a = ap.parse_args()

    if TTFont is None:
        print("monofont: needs fontTools (the python3-fonttools port, or any fontTools 4)", file=sys.stderr)
        return 2
    if a.selftest:
        import tempfile
        with tempfile.TemporaryDirectory() as tmp:
            return selftest(tmp)
    if a.root and not os.path.isdir(os.path.join(a.root, "usr/share/fonts")):
        print("monofont: %s has no usr/share/fonts" % a.root, file=sys.stderr)
        return 2

    path = a.face or find_face(a.family, a.root)
    if not path or not os.path.isfile(path):
        print("monofont: no Regular face of %r %s" % (a.family, "in " + a.root if a.root else "on this host"),
              file=sys.stderr)
        return 2
    fb_paths = list(a.fallback_face)
    if not a.fallback_face:
        chain = a.fallback
        if chain is None:
            prefer, conf = shipped_chain(a.root)
            if not conf:
                print("monofont: no %s ships a monospace chain; name --fallback" % MONO_CONF, file=sys.stderr)
                return 2
            chain = chain_after(prefer, a.family)
            if chain is None:
                print("monofont: %s does not put %r first for monospace (it names %s)" %
                      (conf, a.family, ", ".join(prefer) or "nothing"), file=sys.stderr)
                return 1
        for fam in chain:
            p = find_face(fam, a.root)
            if not p:
                print("monofont: no Regular face of fallback %r" % fam, file=sys.stderr)
                return 2
            fb_paths.append(p)
    try:
        primary = Face(path)
        fallbacks = [Face(p) for p in fb_paths]
    except Exception as e:
        print("monofont: cannot read a face: %s" % e, file=sys.stderr)
        return 2

    r = report(primary, fallbacks)
    if a.json:
        print(json.dumps(r, indent=1))
    else:
        print_report(r)
    return 1 if r["fail"] else 0


if __name__ == "__main__":
    sys.exit(main())
