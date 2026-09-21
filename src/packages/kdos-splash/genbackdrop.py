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
# The boot menu's backdrop: kdos-backdrop.png, drawn from the banner.
#
# HOST ONLY, and its output is committed -- the arrangement genbanner.py,
# genlogo.py and kdos-bb's genimg.py already use. fs/ is copied verbatim into
# the rootfs, so the GENERATOR must not live beside its artefact or the target
# would gain a python script on a system that deliberately has none.
#
# WHY A SECOND PICTURE RATHER THAN THE BANNER ITSELF. Limine draws a
# `centered` wallpaper at its own size in the middle of the screen, which is
# exactly where it draws the menu, so the banner's wordmark landed behind the
# entry text. The menu is `stretched` over this instead -- and a 960x280 banner
# stretched to a 16:9 screen is a smeared wordmark, so the artwork has to be
# composed at the aspect it will be shown at.
#
# IT IS ACHROMATIC, AND THAT IS NOT A STYLE CHOICE. Limine cannot retint a
# wallpaper and `kdos-bootctl theme` rewrites text keys only -- it will not
# replace a PNG on the ESP, because a theme change must not be a write of
# artwork to a FAT filesystem that may be half-done when the power goes. So
# this one file has to sit under every accent, and the only picture that does
# is one with no hue of its own. Under bone it reads as texture; under phosphor
# the menu's own green sits on it and it still reads as texture.
#
# THE MENU PLATE IS TRANSPARENT, SO THIS PICTURE IS THE WHOLE SCREEN.
# `term_background` carries a transparency byte and it is set to `ff` — see
# kcol_limine_conf(). An OPAQUE plate covers everything but `term_margin`, so
# the wordmark had to squeeze into a hundred-pixel band and the plate's edge
# drew a visible rectangle around the menu wherever the band was not exactly
# the plate's colour. Transparent, there is no plate and no rectangle: the
# entries are drawn straight onto this, and the wordmark can be as large as
# the space above them allows.
#
# WHICH IS WHY THE FIELD IS FLAT. Every gradient, vignette and texture that
# was here before existed to make a hundred-pixel border look deliberate. With
# no border there is nothing for them to do, and what they actually
# contributed was banding in the corners: the wordmark folded onto itself
# reads as grain at full size and as smudges at a sixth of it. A flat floor
# cannot band.
#
# THE BANNER IS PLACED AT 1:1 AND NEVER RESAMPLED. It is PIXEL ART — the
# wordmark is chunky blocks with hard edges — and resampling pixel art with a
# photographic filter is what makes the blocks come out different widths with
# soft jagged edges. SIZE is 1920x1080 because that is what `KDOS_RES`
# defaults to, so on the common screen Limine's `stretched` is 1:1 and every
# block lands on the pixel it was drawn on. Other resolutions are stretched
# and soften; nothing can be done about that from here, and an artwork that is
# right at one resolution beats one that is wrong at all of them.
#
# THE INK IS THE MAX CHANNEL, NOT THE LUMINANCE. The wordmark is phosphor
# green — (57, 255, 20) — which Rec.601 puts at 169 while the penguin's white
# lands at 249, so a plain greyscale conversion leaves the letters visibly
# duller than the mascot beside them. Taking the largest channel puts both at
# the top of the range, which is what "achromatic" was supposed to mean.
#
# AND THE BLACK POINT IS 64, WHICH IS WHAT STOPS THE RECTANGLE. The banner has
# an ambient green glow over the whole image: its corner pixel is (18, 64, 31),
# not black. Composited onto the floor, that glow lifts the banner's whole area
# above the field and draws a visible box around it. 64 is measured — it is the
# 95th percentile of the border ring, and subtracting it takes the border to
# exactly zero while costing 2% of the strong ink.

# IT IS ACHROMATIC. Limine cannot retint a wallpaper, so this one file sits
# under every accent and a green penguin would be green under amber.
# Greyscale reads as engraved under all eight.

#   ./genbackdrop.py            rewrite the backdrop in place
#   ./genbackdrop.py --check    report what it is now and exit

import os
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
BOOT = os.path.join(ROOT, "fs", "usr", "share", "kdos", "boot")
SRC = os.path.join(BOOT, "kdos-banner.png")
OUT = os.path.join(BOOT, "kdos-backdrop.png")

# 16:9, and large enough that a 4K panel upscales rather than a 1080p one
# downscales into aliasing. It is two flat-ish colours and a blurred wordmark,
# so it compresses to a few kilobytes at this size.
SIZE = (1920, 1080)

# The floor, and the colour `term_foreground` is read against. Near-black and
# very slightly warm, so it is the scheme's `deep` and the screen is one field
# rather than a picture with a terminal sitting on it.
FLOOR = (10, 10, 9)

# THE WHOLE BANNER, frame and captions and all. It is one designed piece and
# cropping it to the wordmark threw away the two lines that say what this is.
CROP = None

# What the border ring measures at, and what is subtracted to take it to zero.
# See the header: below this the banner draws a box on the field.
BLACK_POINT = 64

# Where the banner's top edge sits, as a fraction of the height. Fractions
# because the wallpaper is `stretched`, so this holds at every resolution.
#
# THE ONE RULE IS THAT NOTHING REACHES THE MIDDLE. Limine centres the entry
# list vertically whatever the margin is; at 5.5% the banner ends at 31% and
# the list starts at 50%.
BAND_TOP = 0.055

# The hero's brightest pixel. Not 255: pure white is structure — an outline, a
# highlight — everywhere else in this distribution, and a wordmark that reaches
# it has nothing left to be drawn against.
HERO_PEAK = 235


def build() -> Image.Image:
    src = Image.open(SRC).convert("RGB")
    if CROP:
        src = src.crop(CROP)

    # The largest channel, floored at the border's own level and stretched to
    # the peak — see the header for why each of those three is not the obvious
    # choice.
    # int32, NOT int16: the stretch multiplies by HERO_PEAK before dividing,
    # and 191 * 235 overflows a signed 16-bit lane into negative values that
    # come back as garbage through the uint8 cast. The peak lands at 181
    # instead of 245 and the whole banner is dim.
    a = np.array(src).max(axis=2).astype(np.int32)
    a = np.clip(a - BLACK_POINT, 0, None)
    a = (a * HERO_PEAK) // (255 - BLACK_POINT)
    art = Image.fromarray(a.astype(np.uint8), "L")

    lum = Image.new("L", SIZE, 0)
    w, h = art.size
    if w > SIZE[0] or h > SIZE[1]:
        raise SystemExit("genbackdrop: the banner does not fit the canvas")
    # PASTED, NOT RESIZED. See the header.
    lum.paste(art, ((SIZE[0] - w) // 2, int(SIZE[1] * BAND_TOP)))

    # ADDED TO THE FLOOR RATHER THAN PASTED OVER IT, so the artwork can only
    # ever contribute light and the field stays exactly one colour everywhere
    # the artwork is black.
    return Image.merge("RGB", [
        Image.eval(lum, lambda v, f=f: min(255, v + f)) for f in FLOOR
    ])


def main() -> int:
    if not os.path.exists(SRC):
        print(f"genbackdrop: no banner at {SRC}", file=sys.stderr)
        return 1

    if "--check" in sys.argv:
        if not os.path.exists(OUT):
            print("genbackdrop: no backdrop generated yet")
            return 1
        im = Image.open(OUT)
        ex = im.convert("RGB").getextrema()
        print(f"{OUT}: {im.size[0]}x{im.size[1]} {im.mode}")
        print(f"  channel extrema {ex}")
        return 0

    build().save(OUT, optimize=True)
    print(f"wrote {OUT} ({os.path.getsize(OUT)} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
