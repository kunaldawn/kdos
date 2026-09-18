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
# AND IT IS DIM IN THE FILE, because Limine has no wallpaper opacity. How dark
# the backdrop is belongs here and nowhere else.
#
#   ./genbackdrop.py            rewrite the backdrop in place
#   ./genbackdrop.py --check    report what it is now and exit

import os
import sys

from PIL import Image, ImageEnhance, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
BOOT = os.path.join(ROOT, "fs", "usr", "share", "kdos", "boot")
SRC = os.path.join(BOOT, "kdos-banner.png")
OUT = os.path.join(BOOT, "kdos-backdrop.png")

# 16:9, and large enough that a 4K panel upscales rather than a 1080p one
# downscales into aliasing. It is two flat-ish colours and a blurred wordmark,
# so it compresses to a few kilobytes at this size.
SIZE = (1920, 1080)

# The floor the artwork is composited onto. Near-black and very slightly warm,
# so it is not the same value as the terminal plate above it -- a backdrop that
# matched `deep` exactly would make the margin look like a rendering fault
# rather than like a margin.
FLOOR = (10, 10, 9)

# 8%: measured as the point where the wordmark is still discernible as a shape
# on a dark panel and contributes nothing readable behind the menu plate. Above
# about 15% it starts to compete with the entry text at the screen edges; below
# about 4% it is indistinguishable from a flat fill and the picture is dead
# weight on the ESP.
BRIGHT = 0.08

# Enough blur that no edge in the artwork reads as a line somebody might
# mistake for chrome, and not so much that it is a grey cloud.
BLUR = 6.0


def build() -> Image.Image:
    src = Image.open(SRC).convert("RGB")

    # COVER, NOT FIT: the banner is 960x280 and the target is 16:9, so fitting
    # it would letterbox and stretching it would smear the wordmark. Scale to
    # cover and crop the overflow, which keeps the letterforms' proportions.
    sw, sh = src.size
    scale = max(SIZE[0] / sw, SIZE[1] / sh)
    grown = src.resize((max(1, round(sw * scale)), max(1, round(sh * scale))),
                       Image.LANCZOS)
    left = (grown.size[0] - SIZE[0]) // 2
    top = (grown.size[1] - SIZE[1]) // 2
    art = grown.crop((left, top, left + SIZE[0], top + SIZE[1]))

    art = art.convert("L").convert("RGB")       # no hue of its own
    art = art.filter(ImageFilter.GaussianBlur(BLUR))
    art = ImageEnhance.Brightness(art).enhance(BRIGHT)

    # ADDED TO THE FLOOR RATHER THAN PASTED OVER IT, so the artwork can only
    # ever contribute light. Pasted, its black areas would punch the floor down
    # to zero and the margin gradient would end at a different colour from the
    # field it surrounds.
    return Image.merge("RGB", [
        Image.eval(chan, lambda v, f=floor: min(255, v + f))
        for chan, floor in zip(art.split(), FLOOR)
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
