#!/usr/bin/env python3
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   genaddons — the colour schemes and wallpapers of the add-on set
# ---------------------------------
#
#   genaddons.py --kcolor kcolor.h --palettes palettes.txt --out DIR
#   genaddons.py --kcolor kcolor.h --palettes palettes.txt --report
#
# Run by build.sh at build time, with python3 alone; nothing it writes is
# committed. DIR is the package's usr/share, and it gets:
#
#   konsole/<Id>.colorscheme             one per palettes.txt block
#   color-schemes/<Id>.colors            the same palette as a KDE scheme
#   wallpapers/org.kdos.<name>/          four Plasma wallpaper packages, each
#                                        an SVG for light (images/) and dark
#                                        (images_dark/), drawn in the KDOS
#                                        accents read from libkcolor's table
#
# EVERY TEXT ROLE OF A KDE SCHEME CLEARS WCAG AA, 4.5:1, against both
# backgrounds of the set it is drawn in, and every decoration 3:1. A
# palette's published colour that falls short is moved toward the set's own
# text in 1% steps until it passes, so the scheme a person picks is always
# readable and never further from the palette than it has to be. --report
# prints each moved role with its ratio before and after; a role that cannot
# be brought to the floor stops the build. The view's text holds the same
# 4.5:1 on the selection, solid and laid over the view as Dolphin lays it, and
# the selection is walked toward the view's background until it does.
#
# The wallpapers take their colours from kcolor.h's KCOL_SCHEMES, the one
# table every KDOS colour is generated from, so an accent retuned there is
# retuned here at the next build. Their directories are never named KDOS-*:
# `kdos theme` rewrites a desktop's wallpaper to the new accent's package only
# while it names a KDOS-* one, and an add-on picked by hand must survive an
# accent switch.

import argparse
import json
import math
import os
import re
import sys

W, H = 3840, 2160
AA, DECO = 4.5, 3.0
# Dolphin's and Breeze's translucent item selection: the Selection laid this
# far over the view, under the view's own text.
SEL_BLEND = 0.64


# ── colour arithmetic ────────────────────────────────────────────────────

def rgb(h):
    h = h.strip().lstrip("#")
    if not re.fullmatch(r"[0-9A-Fa-f]{6}", h):
        raise ValueError("not a colour: %r" % h)
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))


def hexs(c):
    return "#%02x%02x%02x" % c


def csv(c):
    return "%d,%d,%d" % c


def mix(a, b, t):
    """t of the way from a to b."""
    return tuple(int(round(x + (y - x) * t)) for x, y in zip(a, b))


def luminance(c):
    def ch(v):
        v /= 255.0
        return v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4
    r, g, b = (ch(v) for v in c)
    return 0.2126 * r + 0.7152 * g + 0.0722 * b


def contrast(a, b):
    la, lb = luminance(a), luminance(b)
    hi, lo = max(la, lb), min(la, lb)
    return (hi + 0.05) / (lo + 0.05)


def worst(c, grounds):
    return min(contrast(c, g) for g in grounds)


def solve(c, grounds, toward, floor):
    """c moved toward `toward` in 1% steps until it clears `floor` on every
    ground. Returns (colour, steps); None when even `toward` does not."""
    for step in range(0, 101):
        cand = mix(c, toward, step / 100.0)
        if worst(cand, grounds) >= floor:
            return cand, step
    return None, 100


def legible_on(grounds):
    """Black or white, whichever reads better on every ground."""
    black, white = (0, 0, 0), (255, 255, 255)
    return black if worst(black, grounds) >= worst(white, grounds) else white


# ── inputs ───────────────────────────────────────────────────────────────

KCOL_FIELDS = ("primary", "dim", "secondary", "urgent", "deep", "text", "variant", "pdark", "backdrop")
KCOL_ROW = re.compile(r'X\(\s*(\w+)\s*,\s*"([^"]*)"\s*,\s*"([^"]*)"\s*,\s*([^)]*)\)')


def read_kcolor(path):
    """The accents of libkcolor's KCOL_SCHEMES X-macro, by name."""
    with open(path, encoding="utf-8") as f:
        text = f.read()
    m = re.search(r"#define\s+KCOL_SCHEMES\(X\)(.*?)(?:\n\s*\n|\n#)", text, re.S)
    if not m:
        raise ValueError("%s: no KCOL_SCHEMES table" % path)
    body = m.group(1).replace("\\\n", " ")
    out = {}
    for name, label, theme, vals in KCOL_ROW.findall(body):
        hexes = [v.strip() for v in vals.split(",")]
        if len(hexes) != len(KCOL_FIELDS):
            raise ValueError("%s: %s has %d colours" % (path, name, len(hexes)))
        out[name] = dict(zip(KCOL_FIELDS, (rgb(h) for h in hexes)), label=label)
    if not out:
        raise ValueError("%s: KCOL_SCHEMES is empty" % path)
    return out


ROLES = ("window", "view", "alternate", "button", "button_text", "header", "text",
         "selection", "selection_text", "link", "visited", "negative", "neutral", "positive")


def read_palettes(path):
    pals, cur = [], None
    with open(path, encoding="utf-8") as f:
        for n, raw in enumerate(f, 1):
            s = raw.strip()
            if not s or s.startswith("#"):
                continue
            if s.startswith("[") and s.endswith("]"):
                cur = {"id": s[1:-1], "line": n}
                pals.append(cur)
                continue
            if cur is None or "=" not in s:
                raise ValueError("%s:%d: not inside a [palette] block, or no '='" % (path, n))
            k, v = (x.strip() for x in s.split("=", 1))
            cur[k] = v
    for p in pals:
        where = "%s: [%s]" % (path, p["id"])
        if p["id"].lower().startswith("kdos"):
            raise ValueError("%s: an add-on is never named KDOS*, the accents' namespace" % where)
        for k in ("name", "licence", "copyright", "source", "ansi", "background", "foreground"):
            if k not in p:
                raise ValueError("%s: no %s" % (where, k))
        p["dark"] = p.get("dark", "yes") == "yes"
        ansi = p["ansi"].split()
        if len(ansi) != 16:
            raise ValueError("%s: ansi has %d colours, not 16" % (where, len(ansi)))
        p["ansi"] = [rgb(a) for a in ansi]
        for k in ("background", "foreground") + ROLES:
            if k in p:
                p[k] = rgb(p[k])
    return pals


def file_id(p):
    """Nord, GruvboxDark: the palette's name without its spaces."""
    return re.sub(r"[^A-Za-z0-9]", "", p["name"])


# ── Konsole ──────────────────────────────────────────────────────────────

def konsole(p):
    bg, fg = p["background"], p["foreground"]
    if contrast(fg, bg) < AA:
        raise ValueError("[%s] foreground on background is %.2f:1, under %.1f"
                         % (p["id"], contrast(fg, bg), AA))
    out = []

    def group(name, c):
        out.append("[%s]\nColor=%s\n" % (name, csv(c)))

    group("Background", bg)
    group("BackgroundFaint", bg)
    group("BackgroundIntense", mix(bg, fg, 0.08))
    for i in range(8):
        base, bright = p["ansi"][i], p["ansi"][i + 8]
        group("Color%d" % i, base)
        group("Color%dFaint" % i, mix(base, bg, 0.33))
        group("Color%dIntense" % i, bright)
    group("Foreground", fg)
    group("ForegroundFaint", mix(fg, bg, 0.33))
    group("ForegroundIntense", p["ansi"][15] if p["dark"] else mix(fg, (0, 0, 0), 0.3))
    out.append("[General]\nAnchor=0.5,0.5\nBlur=false\nColorRandomization=false\n"
               "Description=%s\nFillStyle=Tile\nOpacity=1\nWallpaper=\n"
               "WallpaperFlipType=NoFlip\nWallpaperOpacity=1\n" % p["name"])
    return "\n".join(out)


# ── KDE colour scheme ────────────────────────────────────────────────────

def derive(p):
    """Every role, the palette's own where it gives one."""
    r = dict(p)
    r.setdefault("window", p["background"])
    r.setdefault("view", p["background"])
    r.setdefault("text", p["foreground"])
    lift = (255, 255, 255) if p["dark"] else (0, 0, 0)
    r.setdefault("alternate", mix(r["view"], lift, 0.04))
    r.setdefault("button", mix(r["window"], lift, 0.08))
    r.setdefault("button_text", r["text"])
    r.setdefault("header", r["window"])
    r.setdefault("selection", p["ansi"][4])
    r.setdefault("selection_text", legible_on([r["selection"]]))
    r.setdefault("link", p["ansi"][12])
    r.setdefault("visited", p["ansi"][13])
    r.setdefault("negative", p["ansi"][9])
    r.setdefault("neutral", p["ansi"][11])
    r.setdefault("positive", p["ansi"][10])
    return r


def colorset(r, bg, alt, text, moved, label):
    """One [Colors:*] group: every role solved against bg and alt."""
    grounds = [bg, alt]
    fg, steps = solve(text, grounds, legible_on(grounds), AA)
    if fg is None:
        raise ValueError("%s text cannot reach %.1f" % (label, AA))
    if steps:
        moved.append((label, "ForegroundNormal", worst(text, grounds), worst(fg, grounds)))

    def role(name, c, floor=AA, base=None):
        start = c if base is None else base
        got, steps = solve(start, grounds if floor == AA else [bg], fg, floor)
        if got is None:
            raise ValueError("%s %s cannot reach %.1f" % (label, name, floor))
        if steps and base is None:
            moved.append((label, name, worst(c, grounds), worst(got, grounds)))
        return got

    accent = r["selection"]
    return [
        ("BackgroundAlternate", alt),
        ("BackgroundNormal", bg),
        ("DecorationFocus", role("DecorationFocus", accent, DECO)),
        ("DecorationHover", role("DecorationHover", accent, DECO)),
        ("ForegroundActive", role("ForegroundActive", accent)),
        ("ForegroundInactive", role("ForegroundInactive", None, base=mix(fg, bg, 0.45))),
        ("ForegroundLink", role("ForegroundLink", r["link"])),
        ("ForegroundNegative", role("ForegroundNegative", r["negative"])),
        ("ForegroundNeutral", role("ForegroundNeutral", r["neutral"])),
        ("ForegroundNormal", fg),
        ("ForegroundPositive", role("ForegroundPositive", r["positive"])),
        ("ForegroundVisited", role("ForegroundVisited", r["visited"])),
    ]


def blend_grounds(selection, view):
    """The view's two backgrounds with the selection laid SEL_BLEND over them."""
    d = dict(view)
    return [mix(d["BackgroundNormal"], selection, SEL_BLEND),
            mix(d["BackgroundAlternate"], selection, SEL_BLEND)]


def solve_selection(r, view, moved):
    """The selection and its ink. KDE draws the view's text over the selection
    as often as the selection's own text: Dolphin's and Breeze's translucent
    item selection keeps the view's label, and so does GTK through Plasma's
    bridge. The palette's selection is kept, under its own ink, while the
    view's text holds 4.5:1 on it solid and laid SEL_BLEND over both view
    backgrounds; otherwise it is walked toward the view's background until it
    does, and the view's text becomes its ink. DecorationFocus keeps the
    palette's selection, the colour a person picked the palette for."""
    text, bg = dict(view)["ForegroundNormal"], dict(view)["BackgroundNormal"]
    published = r["selection"]

    def holds(c):
        return worst(text, [c] + blend_grounds(c, view)) >= AA

    if holds(published) and contrast(r["selection_text"], published) >= AA:
        return published, r["selection_text"]
    for step in range(0, 101):
        cand = mix(published, bg, step / 100.0)
        if holds(cand):
            moved.append(("Selection", "BackgroundNormal", worst(text, [published]), worst(text, [cand])))
            return cand, text
    raise ValueError("the view's text cannot reach %.1f on any selection" % AA)


def kdecolors(p):
    r = derive(p)
    moved = []
    lift = (255, 255, 255) if p["dark"] else (0, 0, 0)
    sets = {
        "View": colorset(r, r["view"], r["alternate"], r["text"], moved, "View"),
        "Window": colorset(r, r["window"], mix(r["window"], lift, 0.05), r["text"], moved, "Window"),
        "Button": colorset(r, r["button"], mix(r["button"], lift, 0.06), r["button_text"], moved, "Button"),
        "Header": colorset(r, r["header"], mix(r["header"], lift, 0.05), r["text"], moved, "Header"),
        "Header][Inactive": colorset(r, r["window"], mix(r["window"], lift, 0.05), r["text"], moved,
                                     "Header][Inactive"),
        "Tooltip": colorset(r, r["alternate"], r["view"], r["text"], moved, "Tooltip"),
        "Complementary": colorset(r, r["header"], r["window"], r["text"], moved, "Complementary"),
    }
    # The selection's alternate row moves away from its text, so the pair
    # that is legible on the selection stays legible on the alternate.
    selection, ink = solve_selection(r, sets["View"], moved)
    away = (0, 0, 0) if luminance(ink) > luminance(selection) else (255, 255, 255)
    sel_alt = mix(selection, away, 0.08)
    sel = colorset(dict(r, selection=ink, link=ink, visited=ink, negative=ink,
                        neutral=ink, positive=ink),
                   selection, sel_alt, ink, moved, "Selection")
    sets["Selection"] = sel

    sid = file_id(p)
    out = ["[ColorEffects:Disabled]\nColor=%s\nColorAmount=0\nColorEffect=0\nContrastAmount=0.65\n"
           "ContrastEffect=1\nIntensityAmount=0.1\nIntensityEffect=2\n" % csv(mix(r["window"], r["text"], 0.2)),
           "[ColorEffects:Inactive]\nChangeSelectionColor=true\nColor=%s\nColorAmount=0.025\nColorEffect=2\n"
           "ContrastAmount=0.1\nContrastEffect=2\nEnable=false\nIntensityAmount=0\nIntensityEffect=0\n"
           % csv(mix(r["window"], r["text"], 0.45))]
    for name in ("Button", "Complementary", "Header", "Header][Inactive", "Selection", "Tooltip", "View", "Window"):
        out.append("[Colors:%s]\n" % name + "".join("%s=%s\n" % (k, csv(v)) for k, v in sets[name]))
    hdr, win = dict(sets["Header"]), dict(sets["Window"])
    out.append("[General]\nColorScheme=%s\nName=%s\nshadeSortColumn=true\n" % (sid, p["name"]))
    out.append("[KDE]\ncontrast=4\n")
    out.append("[WM]\nactiveBackground=%s\nactiveBlend=%s\nactiveForeground=%s\n"
               "inactiveBackground=%s\ninactiveBlend=%s\ninactiveForeground=%s\n"
               % (csv(hdr["BackgroundNormal"]), csv(hdr["ForegroundNormal"]), csv(hdr["ForegroundNormal"]),
                  csv(win["BackgroundNormal"]), csv(win["ForegroundInactive"]), csv(win["ForegroundInactive"])))
    return "\n".join(out), sets, moved


def audit_sets(label, sets):
    """Every text role at 4.5 on both backgrounds, every decoration at 3, and
    the view's text at 4.5 on the selection, solid and blended."""
    bad = []
    view, sel = dict(sets["View"]), dict(sets["Selection"])["BackgroundNormal"]
    for g in [sel] + blend_grounds(sel, sets["View"]):
        if contrast(view["ForegroundNormal"], g) < AA - 1e-9:
            bad.append("%s [Colors:View] ForegroundNormal on the selection %s is %.2f:1"
                       % (label, csv(g), contrast(view["ForegroundNormal"], g)))
    for name, roles in sets.items():
        d = dict(roles)
        grounds = [d["BackgroundNormal"], d["BackgroundAlternate"]]
        for k, c in d.items():
            if k.startswith("Foreground") and worst(c, grounds) < AA - 1e-9:
                bad.append("%s [Colors:%s] %s is %.2f:1" % (label, name, k, worst(c, grounds)))
            if k.startswith("Decoration") and contrast(c, d["BackgroundNormal"]) < DECO - 1e-9:
                bad.append("%s [Colors:%s] %s is %.2f:1" % (label, name, k, contrast(c, d["BackgroundNormal"])))
    return bad


# ── wallpapers ───────────────────────────────────────────────────────────

def svg(body, defs=""):
    return ('<?xml version="1.0" encoding="UTF-8"?>\n'
            '<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 %d %d">\n'
            '<defs>%s</defs>\n%s</svg>\n' % (W, H, W, H, defs, body))


def lingrad(gid, stops, x1=0, y1=0, x2=0, y2=H):
    s = "".join('<stop offset="%g" stop-color="%s" stop-opacity="%g"/>' % (o, hexs(c), a) for o, c, a in stops)
    return ('<linearGradient id="%s" gradientUnits="userSpaceOnUse" x1="%g" y1="%g" x2="%g" y2="%g">%s'
            '</linearGradient>' % (gid, x1, y1, x2, y2, s))


def radgrad(gid, cx, cy, r, stops):
    s = "".join('<stop offset="%g" stop-color="%s" stop-opacity="%g"/>' % (o, hexs(c), a) for o, c, a in stops)
    return ('<radialGradient id="%s" gradientUnits="userSpaceOnUse" cx="%g" cy="%g" r="%g">%s'
            '</radialGradient>' % (gid, cx, cy, r, s))


def scanlines(ground, top, glow, line, line_alpha):
    defs = (lingrad("ground", [(0, top, 1), (1, ground, 1)]) +
            radgrad("glow", W / 2, H * 0.45, W * 0.45, [(0, glow, 0.18), (0.6, glow, 0.05), (1, glow, 0)]))
    rows = "".join('<rect x="0" y="%d" width="%d" height="2"/>' % (y, W) for y in range(0, H, 6))
    body = ('<rect width="%d" height="%d" fill="url(#ground)"/>\n'
            '<rect width="%d" height="%d" fill="url(#glow)"/>\n'
            '<g fill="%s" fill-opacity="%g">%s</g>\n' % (W, H, W, H, hexs(line), line_alpha, rows))
    return svg(body, defs)


def blueprint(ground, far, ink, edge):
    defs = (lingrad("ground", [(0, far, 1), (1, ground, 1)], 0, 0, W, H) +
            radgrad("vignette", W / 2, H / 2, W * 0.62, [(0, edge, 0), (0.7, edge, 0), (1, edge, 0.45)]))
    minor = "".join("M%d 0V%d" % (x, H) for x in range(0, W + 1, 48)) + \
        "".join("M0 %dH%d" % (y, W) for y in range(0, H + 1, 48))
    major = "".join("M%d 0V%d" % (x, H) for x in range(0, W + 1, 240)) + \
        "".join("M0 %dH%d" % (y, W) for y in range(0, H + 1, 240))
    rings = "".join('<circle cx="%d" cy="%d" r="%d"/>' % (W * 0.68, H * 0.42, r) for r in (240, 480, 720))
    body = ('<rect width="%d" height="%d" fill="url(#ground)"/>\n'
            '<path d="%s" fill="none" stroke="%s" stroke-opacity="0.07" stroke-width="1"/>\n'
            '<path d="%s" fill="none" stroke="%s" stroke-opacity="0.16" stroke-width="2"/>\n'
            '<g fill="none" stroke="%s" stroke-opacity="0.22" stroke-width="2">%s</g>\n'
            '<rect width="%d" height="%d" fill="url(#vignette)"/>\n'
            % (W, H, minor, hexs(ink), major, hexs(ink), hexs(ink), rings, W, H))
    return svg(body, defs)


def circle_slice(cx, cy, r, y1, y2):
    """The part of a circle between two heights, as a closed path with a
    point every 4 px of height, so its rim shows no facets at 4K."""
    y1, y2 = max(y1, cy - r), min(y2, cy + r)
    if y2 <= y1:
        return ""
    steps = max(8, int((y2 - y1) / 4))
    ys = [y1 + (y2 - y1) * i / steps for i in range(steps + 1)]
    half = [math.sqrt(max(r * r - (y - cy) ** 2, 0.0)) for y in ys]
    pts = [(cx + h, y) for h, y in zip(half, ys)] + [(cx - h, y) for h, y in reversed(list(zip(half, ys)))]
    return "M" + "L".join("%.1f %.1f" % p for p in pts) + "Z"


def horizon(sky_top, sky_low, sun_top, sun_low, ground, grid):
    hy = int(H * 0.6)
    cx, cy, r = W / 2, hy - 180, 540
    defs = (lingrad("sky", [(0, sky_top, 1), (1, sky_low, 1)], 0, 0, 0, hy) +
            lingrad("sun", [(0, sun_top, 1), (1, sun_low, 1)], 0, cy - r, 0, hy) +
            lingrad("ground", [(0, ground, 1), (1, sky_top, 1)], 0, hy, 0, H))
    # Solid down to a quarter above the middle, then bands parted by gaps
    # that widen toward the horizon.
    top = cy - r * 0.25
    slices, y, gap = [circle_slice(cx, cy, r, cy - r, top)], top, 6
    while y < hy:
        band = max(56 - gap * 1.5, 12)
        slices.append(circle_slice(cx, cy, r, y + gap, min(y + gap + band, hy)))
        y += gap + band
        gap += 5
    lines = []
    d = 14.0
    while hy + d < H:
        lines.append("M0 %.1fH%d" % (hy + d, W))
        d *= 1.34
    for j in range(-24, 25):
        lines.append("M%.1f %dL%.1f %d" % (cx + j * 14, hy, cx + j * 300, H))
    body = ('<rect width="%d" height="%d" fill="url(#sky)"/>\n'
            '<path d="%s" fill="url(#sun)"/>\n'
            '<rect y="%d" width="%d" height="%d" fill="url(#ground)"/>\n'
            '<path d="%s" fill="none" stroke="%s" stroke-opacity="0.55" stroke-width="2"/>\n'
            '<rect y="%d" width="%d" height="3" fill="%s"/>\n'
            % (W, hy, "".join(slices), hy, W, H - hy, "".join(lines), hexs(grid), hy - 1, W, hexs(grid)))
    return svg(body, defs)


def eight(ground, base, bars):
    """The eight accents as a level meter."""
    heights = (0.55, 0.8, 0.62, 1.0, 0.74, 0.9, 0.5, 0.68)
    bw, gap, tall = 36, 44, 720
    x0 = W / 2 - (len(bars) * bw + (len(bars) - 1) * gap) / 2
    floor = H * 0.78
    defs = radgrad("ground", W / 2, floor, W * 0.7, [(0, base, 1), (1, ground, 1)])
    parts = []
    for i, c in enumerate(bars):
        h = tall * heights[i % len(heights)]
        x = x0 + i * (bw + gap)
        parts.append('<rect x="%.1f" y="%.1f" width="%d" height="%.1f" fill="%s"/>' % (x, floor - h, bw, h, hexs(c)))
        parts.append('<rect x="%.1f" y="%.1f" width="%d" height="%.1f" fill="%s" fill-opacity="0.18"/>'
                     % (x, floor + 16, bw, h * 0.35, hexs(c)))
    body = ('<rect width="%d" height="%d" fill="url(#ground)"/>\n%s\n'
            '<rect x="%.1f" y="%.1f" width="%.1f" height="4" fill="%s" fill-opacity="0.5"/>\n'
            % (W, H, "".join(parts), x0 - 60, floor + 4, len(bars) * (bw + gap) - gap + 120, hexs(bars[0])))
    return svg(body, defs)


WALLPAPERS = (
    ("scanlines", "Scanlines", "A phosphor glow behind fine CRT scanlines"),
    ("blueprint", "Blueprint", "A drafting grid with construction rings"),
    ("horizon", "Horizon", "A banded sun over a perspective grid"),
    ("eight", "Eight", "The eight KDOS accents as a level meter"),
)


def wallpapers(k):
    ph, ice, am, pf, bo, pa = (k[n] for n in ("phosphor", "ice", "amber", "perfect", "borland", "paper"))
    order = [k[n]["primary"] for n in ("phosphor", "amber", "ice", "bone", "norton", "borland", "perfect", "paper")]
    light_bars = [mix(c, (0, 0, 0), 0.25) for c in order]
    return {
        "scanlines": (scanlines(pa["deep"], pa["backdrop"], pa["primary"], pa["primary"], 0.05),
                      scanlines(ph["deep"], ph["variant"], ph["primary"], (0, 0, 0), 0.45)),
        "blueprint": (blueprint(pa["deep"], pa["backdrop"], pa["primary"], pa["dim"]),
                      blueprint(ice["deep"], ice["variant"], ice["primary"], (0, 0, 0))),
        "horizon": (horizon(pa["backdrop"], pa["deep"], am["primary"], am["urgent"], pa["variant"], pa["primary"]),
                    horizon(pf["deep"], pf["variant"], am["primary"], am["urgent"], pf["backdrop"], bo["primary"])),
        "eight": (eight(pa["deep"], pa["variant"], light_bars),
                  eight(k["bone"]["deep"], k["bone"]["variant"], order)),
    }


def wallpaper_metadata(wid, name, desc):
    return json.dumps({"KPlugin": {"Authors": [{"Name": "KDOS"}], "Description": desc,
                                   "Id": "org.kdos." + wid, "License": "MIT", "Name": name}},
                      indent=4) + "\n"


# ── output ───────────────────────────────────────────────────────────────

def write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        f.write(text)


def main():
    ap = argparse.ArgumentParser(description="the add-on colour schemes and wallpapers")
    ap.add_argument("--kcolor", required=True, help="libkcolor's kcolor.h")
    ap.add_argument("--palettes", required=True, help="palettes.txt")
    ap.add_argument("--out", help="the package's usr/share")
    ap.add_argument("--report", action="store_true", help="print every role moved to reach its floor")
    a = ap.parse_args()
    try:
        k = read_kcolor(a.kcolor)
        pals = read_palettes(a.palettes)
        for need in ("phosphor", "amber", "ice", "bone", "norton", "borland", "perfect", "paper"):
            if need not in k:
                raise ValueError("%s has no %s accent" % (a.kcolor, need))
        made, failures = [], []
        for p in pals:
            text, sets, moved = kdecolors(p)
            failures += audit_sets(p["name"], sets)
            made.append((p, konsole(p), text, moved))
    except ValueError as e:
        print("genaddons: %s" % e, file=sys.stderr)
        return 2
    if failures:
        for f in failures:
            print("genaddons: %s" % f, file=sys.stderr)
        return 1
    if a.report:
        for p, _, _, moved in made:
            print("%s (%s, %s)" % (p["name"], p["licence"], p["copyright"]))
            for label, role, before, after in moved:
                print("  [Colors:%s] %-18s %5.2f:1 -> %5.2f:1" % (label, role, before, after))
    if a.out:
        for p, kon, colors, _ in made:
            write(os.path.join(a.out, "konsole", file_id(p) + ".colorscheme"), kon)
            write(os.path.join(a.out, "color-schemes", file_id(p) + ".colors"), colors)
        walls = wallpapers(k)
        for wid, name, desc in WALLPAPERS:
            light, dark = walls[wid]
            base = os.path.join(a.out, "wallpapers", "org.kdos." + wid)
            write(os.path.join(base, "metadata.json"), wallpaper_metadata(wid, name, desc))
            write(os.path.join(base, "contents", "images", "%dx%d.svg" % (W, H)), light)
            write(os.path.join(base, "contents", "images_dark", "%dx%d.svg" % (W, H)), dark)
        print("genaddons: %d Konsole and %d KDE colour schemes, %d wallpapers"
              % (len(made), len(made), len(WALLPAPERS)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
