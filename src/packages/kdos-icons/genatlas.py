#!/usr/bin/env python3
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   genatlas.py — art/ (SVG) -> atlas.kia (PNG blobs)
#
# HOST ONLY, like vendor.py and genmarks.py beside it. Nothing on the target
# and nothing in the build ever runs this: the output is committed, exactly the
# way marks/ is, and for the same reason.
#
# WHY IT EXISTS. libkicon draws pixel icons into the character grid, and there
# is no SVG parser anywhere in this tree — kcol_retint_text is a hex-token
# scanner and that is deliberate. So the rasterising happens once, here, on a
# machine that has one.
#
# WHAT IT DOES NOT DO: recolour. The blobs go in wearing upstream's own
# colours and libkicon tints them at load through kcol_remap — the wallpaper's
# pipeline — so ONE atlas serves all four accents and `kdos theme amber`
# retints it live. An atlas per accent would be four copies of eight megabytes
# in git and a fifth the day somebody adds a scheme.
#
# THE FORMAT, little-endian throughout:
#
#     "KIA1"  u32 count
#     count x { u16 nlen, u16 size, u32 noff, u32 off, u32 len }
#     the names, packed
#     the blobs, deduplicated by content
#
# sorted by (name, size) so the reader binary-searches and walks a handful of
# rows for "the smallest at or above what I asked for".
# ---------------------------------

import hashlib
import os
import re
import struct
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ART = os.path.join(HERE, "art")
OUT = os.path.join(HERE, "atlas", "atlas.kia")

# The sizes worth carrying. A cell is 16x32 on this desktop, so one icon is two
# cells wide and one tall (32x32); 64 is that at output scale 2, and 24 is what
# a one-cell indicator gets. 16 and 22 are dropped: they cover 5% more names
# than 24 does and cost a third of the file.
SIZES = [24, 32, 48, 64]

# Context priority. The atlas is keyed by NAME with no context, the way every
# consumer asks for one — and `bluetooth` is in devices AND status, `folder` in
# places AND mimetypes. First context to claim a name at a size keeps it.
CONTEXTS = ["places", "devices", "status", "mimetypes", "actions", "emblems"]


# Papirus's smaller icons are written the KDE way: `fill:currentColor` plus a
# `<style>` block that sets `color:` on a class. ImageMagick's SVG path does not
# resolve `currentColor` through a CSS class and renders them EMPTY — 24x24
# `settings`, `help` and `system-lock-screen` all came out as 81-byte fully
# transparent PNGs, which is worse than having no icon at all: an empty picture
# still takes the slot and the caller never falls back to its glyph.
#
# So the colour is substituted before rasterising, and anything that comes out
# transparent anyway is DROPPED rather than shipped.
CURRENT_COLOR_DEFAULT = "#dfdfdf"


def prepare(svg, tmp):
    """The SVG to rasterise — the file itself, or a fixed-up copy."""
    try:
        with open(svg, "r", encoding="utf-8", errors="replace") as f:
            text = f.read()
    except OSError:
        return svg
    if "currentColor" not in text:
        return svg
    m = re.search(r"\.ColorScheme-Text\s*\{\s*color:\s*(#[0-9a-fA-F]{3,8})",
                  text)
    colour = m.group(1) if m else CURRENT_COLOR_DEFAULT
    fixed = os.path.join(tmp, "fixed.svg")
    with open(fixed, "w", encoding="utf-8") as f:
        f.write(text.replace("currentColor", colour))
    return fixed


def is_blank(path):
    """True when every pixel is transparent. Only asked of a suspiciously
    small file: `identify` per icon over ten thousand icons is minutes."""
    try:
        r = subprocess.run(["identify", "-format", "%[fx:maxima.a]", path],
                           capture_output=True, text=True, check=True)
        return float(r.stdout.strip() or "0") == 0.0
    except (subprocess.CalledProcessError, FileNotFoundError, ValueError):
        return False


def rasterise(svg, px, tmp):
    """One SVG at one size, as PNG bytes. None if anything went wrong — and
    None, deliberately, for a picture that rendered to nothing."""
    out = os.path.join(tmp, "o.png")
    src = prepare(svg, tmp)
    try:
        subprocess.run(
            ["convert", "-background", "none", src, "-resize",
             "%dx%d" % (px, px), "-strip", "png32:" + out],
            check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    except (subprocess.CalledProcessError, FileNotFoundError):
        return None
    try:
        size = os.path.getsize(out)
        if size < 400 and is_blank(out):
            return None
        with open(out, "rb") as f:
            return f.read()
    except OSError:
        return None


def main():
    if not os.path.isdir(ART):
        sys.exit("genatlas: no art/ — run vendor.py first")

    entries = []            # (name, size, blob_digest)
    blobs = {}              # digest -> bytes
    seen = set()            # (name, size)
    missing_tool = True

    with tempfile.TemporaryDirectory() as tmp:
        for size in SIZES:
            sdir = os.path.join(ART, "%dx%d" % (size, size))
            if not os.path.isdir(sdir):
                continue
            for ctx in CONTEXTS:
                cdir = os.path.join(sdir, ctx)
                if not os.path.isdir(cdir):
                    continue
                for fn in sorted(os.listdir(cdir)):
                    if not fn.endswith(".svg"):
                        continue
                    name = fn[:-4]
                    if (name, size) in seen:
                        continue
                    png = rasterise(os.path.join(cdir, fn), size, tmp)
                    if png is None:
                        continue
                    missing_tool = False
                    d = hashlib.sha256(png).hexdigest()
                    blobs.setdefault(d, png)
                    entries.append((name, size, d))
                    seen.add((name, size))
            print("  %dx%d: %d entries" % (size, size, len(entries)),
                  file=sys.stderr)

    if missing_tool:
        sys.exit("genatlas: `convert` produced nothing — is ImageMagick "
                 "installed, with an SVG delegate?")

    # (name, size) ascending: the reader's binary search depends on it, and so
    # does its "walk forward to the first size >= want" loop.
    entries.sort(key=lambda e: (e[0].encode(), e[1]))

    names = bytearray()
    name_off = {}
    payload = bytearray()
    blob_off = {}

    header = 8 + len(entries) * 16
    for name, _size, _d in entries:
        if name not in name_off:
            name_off[name] = len(names)
            names += name.encode()
    for d, b in blobs.items():
        blob_off[d] = len(payload)
        payload += b

    base_names = header
    base_blobs = header + len(names)

    out = bytearray()
    out += b"KIA1"
    out += struct.pack("<I", len(entries))
    for name, size, d in entries:
        nb = name.encode()
        out += struct.pack("<HHIII", len(nb), size,
                           base_names + name_off[name],
                           base_blobs + blob_off[d], len(blobs[d]))
    out += names
    out += payload

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "wb") as f:
        f.write(out)

    print("genatlas: %d entries, %d unique pictures, %.1f MB -> %s"
          % (len(entries), len(blobs), len(out) / 1048576.0, OUT))


if __name__ == "__main__":
    main()
