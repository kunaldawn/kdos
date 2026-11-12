#!/usr/bin/env python3
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   genstatus.py — the KDOS theme's status and emblem icons, from Breeze Dark
#
#   genstatus.py --src /usr/share/icons/breeze-dark --out themes/KDOS
#
# Run on a development host against an installed breeze-dark; the build only
# copies what it committed. Breeze draws its status, emblem and package
# marks as a shape KIconLoader recolours (ColorScheme-Accent, -Highlight,
# -PositiveText, -NeutralText or -NegativeText) under a glyph that is fixed
# white. In every dark KDOS accent those shape colours are light, so the
# glyph vanishes. Each such icon is copied with its opaque white glyph drawn
# in the scheme colour the shape's colour is solved against instead:
# ColorScheme-Background on a Positive, Neutral or Negative shape (those
# roles hold 4.5:1 on the Window background), ColorScheme-HighlightedText on
# an Accent or Highlight shape (the Selection's text holds 4.5:1 on it).
#
# A white element drawn translucent is a highlight, not a glyph, and is left
# white; `places` is not read, because a folder's white is only highlights.
# The aliases breeze-dark links to a copied icon are linked again beside the
# copy, since a lookup by an alias would otherwise reach the white original,
# and so are the @2x and @3x directories of each directory written.
#
# A name breeze-dark draws in more than one context is neither copied nor
# linked: `checkmark` is both an action (dialog-ok-apply) and an emblem
# (emblem-checked), and breeze-dark lists actions first, so a request for it
# gets the plain tick. A copy here would be the only `checkmark` in the top
# theme, and KIconLoader, which tries a name and its dash-stripped forms in
# each theme before the next, would answer `checkmark` and
# `checkmark-symbolic` with the filled emblem.
#
# For the same reason every breeze-dark name that extends a copied one
# (`printer-error-symbolic`, `package-install-auto`) is linked here to
# breeze-dark's own file: without the link a request for it is answered by
# the copy of the shorter name before breeze-dark is searched.
#
# Everything in index.theme below MARKER is rewritten, and the Directories
# and ScaledDirectories keys are this generator's entries after the ones
# above the marker names; edit nothing below it by hand.
# ---------------------------------

import argparse
import os
import re
import shutil
import sys

SHAPES = {
    "Accent": "HighlightedText",
    "Highlight": "HighlightedText",
    "PositiveText": "Background",
    "NeutralText": "Background",
    "NegativeText": "Background",
}

# Breeze Dark's own defaults for the two classes, for a renderer that does no
# recolouring; KIconLoader replaces the whole stylesheet.
DEFAULT = {"Background": "#2a2e32", "HighlightedText": "#fcfcfc"}

MARKER = "# genstatus.py writes everything below this line"
SKIP = {"places"}

TAG = re.compile(r"<(path|rect|circle|ellipse|polygon|polyline|use)\b[^>]*>", re.S)
WHITE = r"(?:#fff(?:fff)?|white)"


def opaque(tag):
    for attr in ("opacity", "fill-opacity"):
        m = re.search(r'\s%s="([0-9.]+)"' % attr, tag)
        if m and float(m.group(1)) < 1:
            return False
    m = re.search(r'style="([^"]*)"', tag)
    if m:
        for v in re.findall(r"(?:^|;)\s*(?:fill-)?opacity\s*:\s*([0-9.]+)", m.group(1)):
            if float(v) < 1:
                return False
    return True


def white(tag):
    if re.search(r'\sfill="%s"' % WHITE, tag, re.I):
        return True
    m = re.search(r'style="([^"]*)"', tag)
    return bool(m and re.search(r"(?:^|;)\s*fill\s*:\s*%s\s*(?:;|$)" % WHITE, m.group(1), re.I))


def recolour(svg):
    """The SVG with its white glyphs in the scheme's class, or None."""
    shape = None
    for m in re.finditer(r'class="ColorScheme-([A-Za-z]+)"', svg):
        if m.group(1) in SHAPES:
            shape = m.group(1)
            break
    if not shape or 'id="current-color-scheme"' not in svg:
        return None
    cls = SHAPES[shape]
    hits = 0

    def fix(m):
        nonlocal hits
        tag = m.group(0)
        if "class=" in tag or not white(tag) or not opaque(tag):
            return tag
        hits += 1
        tag = re.sub(r'(\s)fill="%s"' % WHITE, r'\1fill="currentColor"', tag, flags=re.I)
        tag = re.sub(r"(fill\s*:\s*)%s" % WHITE, r"\1currentColor", tag, flags=re.I)
        return re.sub(r"^<(\w+)", r'<\1 class="ColorScheme-%s"' % cls, tag)

    out = TAG.sub(fix, svg)
    if not hits:
        return None
    rule = ".ColorScheme-%s { color: %s; }" % (cls, DEFAULT[cls])
    if rule not in out:
        out = re.sub(r'(id="current-color-scheme"[^>]*>)', r"\1" + rule + " ", out, count=1)
        if rule not in out:
            return None
    return out


def sections(index):
    """{dir: [lines]} for every directory section of an index.theme."""
    out, cur = {}, None
    for line in open(index, encoding="utf-8"):
        line = line.rstrip("\n")
        m = re.match(r"^\[(.+)\]$", line)
        if m:
            cur = m.group(1) if m.group(1) != "Icon Theme" else None
            if cur:
                out[cur] = []
            continue
        if cur and re.match(r"^[A-Za-z]+=", line):
            out[cur].append(line)
    return out


def key(index, name):
    for line in open(index, encoding="utf-8"):
        if line.startswith(name + "="):
            return [d for d in line.strip().split("=", 1)[1].split(",") if d]
    return []


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", required=True, help="an installed breeze-dark")
    ap.add_argument("--out", required=True, help="themes/KDOS")
    a = ap.parse_args()

    src_index = os.path.join(a.src, "index.theme")
    src_dirs = key(src_index, "Directories")
    src_scaled = set(key(src_index, "ScaledDirectories"))
    src_sec = sections(src_index)

    index = os.path.join(a.out, "index.theme")
    text = open(index, encoding="utf-8").read()
    if MARKER not in text:
        sys.exit("genstatus: %s has no marker line" % index)
    head = text.split(MARKER)[0]
    m = re.search(r"^Directories=(.*)$", head, re.M)
    keep = [d for d in m.group(1).split(",") if d] if m else []

    contexts = {}
    for d in src_dirs:
        sd = os.path.join(a.src, d)
        if os.path.isdir(sd):
            for f in os.listdir(sd):
                contexts.setdefault(f, set()).add(d.split("/")[0])

    written = []
    for d in src_dirs:
        if d.split("/")[0] in SKIP or "@" in d:
            continue
        sd = os.path.join(a.src, d)
        if not os.path.isdir(sd):
            continue
        od = os.path.join(a.out, d)
        shutil.rmtree(od, ignore_errors=True)
        done = set()
        for f in sorted(os.listdir(sd)):
            p = os.path.join(sd, f)
            if not f.endswith(".svg") or os.path.islink(p) or len(contexts[f]) > 1:
                continue
            new = recolour(open(p, encoding="utf-8").read())
            if new is None:
                continue
            os.makedirs(od, exist_ok=True)
            with open(os.path.join(od, f), "w", encoding="utf-8") as o:
                o.write(new)
            done.add(f)
        if not done:
            continue
        for f in sorted(os.listdir(sd)):
            p = os.path.join(sd, f)
            if os.path.islink(p) and os.readlink(p) in done and len(contexts[f]) == 1:
                os.symlink(os.readlink(p), os.path.join(od, f))
        written.append(d)
        for s in ("@2x", "@3x"):
            if d + s in src_scaled:
                link = os.path.join(a.out, d + s)
                if os.path.lexists(link):
                    os.remove(link)
                os.symlink(os.path.basename(d), link)

    copied = set()
    for d in written:
        copied.update(os.listdir(os.path.join(a.out, d)))
    for d in src_dirs:
        if d.split("/")[0] in SKIP or "@" in d:
            continue
        sd = os.path.join(a.src, d)
        if not os.path.isdir(sd):
            continue
        for f in sorted(os.listdir(sd)):
            if f in copied or not any(f.startswith(c[:-4] + "-") for c in copied):
                continue
            od = os.path.join(a.out, d)
            os.makedirs(od, exist_ok=True)
            up = "/".join([".."] * (d.count("/") + 2))
            os.symlink("%s/%s/%s/%s" % (up, os.path.basename(a.src.rstrip("/")), d, f),
                       os.path.join(od, f))
            if d not in written:
                written.append(d)
                for s in ("@2x", "@3x"):
                    if d + s in src_scaled:
                        link = os.path.join(a.out, d + s)
                        if os.path.lexists(link):
                            os.remove(link)
                        os.symlink(os.path.basename(d), link)

    scaled = [d + s for d in written for s in ("@2x", "@3x") if d + s in src_scaled]
    dirs = [d for d in keep if d not in written] + written
    head = re.sub(r"^Directories=.*$", "Directories=" + ",".join(dirs), head, flags=re.M)
    head = re.sub(r"^ScaledDirectories=.*\n", "", head, flags=re.M)
    if scaled:
        head = re.sub(r"^(Directories=.*)$", r"\1\nScaledDirectories=" + ",".join(scaled),
                      head, flags=re.M)
    body = []
    for d in written + scaled:
        body.append("\n[%s]\n%s\n" % (d, "\n".join(src_sec[d])))
    with open(index, "w", encoding="utf-8") as o:
        o.write(head + MARKER + "\n" + "".join(body))
    n = sum(len([f for f in os.listdir(os.path.join(a.out, d))]) for d in written)
    print("genstatus: %d directories, %d icons and aliases" % (len(written), n))


if __name__ == "__main__":
    main()
