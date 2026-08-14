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
# Cut the KDOS marks in marks/ out of the master artwork (kdos.png).
#
# MAINTENANCE TOOL, not part of the build: it needs PIL, which the KDOS chroot
# does not have. Run it on the host when kdos.png changes; the build only ever
# reads the PNGs committed in marks/.
#
#   genmarks.py [path/to/kdos.png]
#
# Two marks, cut from the one source so they cannot drift:
#
#   tux    the penguin alone — the dock's app-library button.
#   logo   penguin + wordmark.
#
# The crop uses TWO alpha thresholds and they do different jobs. SOLID finds
# where the artwork proper is, which is what separates the penguin from the
# wordmark below it (the wordmark is a second block of solid pixels with a gap
# of pure glow between). GLOW is what the crop box is actually taken at, so the
# halo survives — cropping at SOLID shaves it off on all four sides, which is
# exactly what it looks like.
# This replaces the old panel-tux.svg, which was <rect> pixel art traced from
# the boot splash's 34x34 penguin because a general-purpose SVG renderer is
# built without raster-image support, so an SVG wrapping a PNG renders empty.
# PNG icons sidestep that entirely — icon themes have always taken them — and
# a Lanczos downscale of the real artwork beats a 34x34 grid at every size.

import os
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
MARKS = os.path.join(HERE, 'marks')
DEFAULT_SRC = os.path.normpath(os.path.join(HERE, '..', '..', '..', 'kdos.png'))

SIZES = (16, 22, 24, 32, 48, 64, 128, 256)

SOLID = 150          # alpha at or above this is artwork, not glow
GLOW = 8             # ...and at or above this is still visible halo
MARGIN = 0.02        # fraction of the square kept clear around the crop


def split_row(im):
    """First row of the wordmark.

    The penguin and the wordmark are two blocks of SOLID pixels separated by a
    band that is glow only, so: walk the solid rows, take the end of the first
    block, then find where the next block starts. Backing off two rows keeps
    the wordmark's own antialiasing out of the penguin's crop.
    """
    import array
    a = im.getchannel('A')
    w, h = im.size
    px = a.load()
    solid = array.array('B', (0,)) * h
    for y in range(h):
        solid[y] = 1 if any(px[x, y] >= SOLID for x in range(0, w, 3)) else 0

    y = 0
    while y < h and not solid[y]:
        y += 1
    while y < h and solid[y]:
        y += 1
    end = y
    while y < h and not solid[y]:
        y += 1
    return h if y >= h else max(end, y - 2)


def crop(im, box_bottom):
    """Tight bbox of everything visible above box_bottom, squared and margined."""
    band = im.crop((0, 0, im.width, box_bottom))
    mask = band.getchannel('A').point(lambda v: 255 if v >= GLOW else 0)
    bb = mask.getbbox()
    if bb is None:
        raise SystemExit('genmarks: no artwork found')
    art = band.crop(bb)
    side = int(max(art.size) * (1 + 2 * MARGIN))
    out = Image.new('RGBA', (side, side), (0, 0, 0, 0))
    out.alpha_composite(art, ((side - art.width) // 2, (side - art.height) // 2))
    return out


def emit(img, name):
    n = 0
    for s in SIZES:
        img.resize((s, s), Image.LANCZOS).save(
            os.path.join(MARKS, '%s-%d.png' % (name, s)))
        n += 1
    return n


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_SRC
    im = Image.open(src).convert('RGBA')
    os.makedirs(MARKS, exist_ok=True)

    cut = split_row(im)
    n = emit(crop(im, cut), 'tux')
    n += emit(crop(im, im.height), 'logo')
    print('kdos-icons: wordmark starts at y=%d; %d mark files -> %s'
          % (cut, n, MARKS))


main()
