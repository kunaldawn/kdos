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
# Restamp the two caption lines on kdos-banner.png, the rEFInd boot banner.
#
# HOST ONLY, and its output is committed -- the arrangement genlogo.py,
# genmarks.py and kdos-bb's genimg.py already use. It exists because the
# banner is the one shipped artefact with TEXT BAKED INTO A BINARY: the
# strings below said `cosmic` for a whole milestone after that desktop was
# deleted, and nothing in the tree could see it. grep cannot read a PNG.
# Changing what the boot screen says is now a one-line edit here.
#
# IDEMPOTENT. The caption band is repainted with the flat background before
# anything is drawn, so running this twice produces the same file. The band
# is measured, not guessed: BAND is the region right of the mascot and above
# the rule that contains the two caption lines and nothing else.
#
#   ./genbanner.py            rewrite the banner in place
#   ./genbanner.py --check    report what it says now and exit

import os, sys
from PIL import Image, ImageDraw, ImageFont

# The banner is shipped through fs/, which is copied verbatim into the
# rootfs -- so the GENERATOR must not live beside it or the target would gain
# a python script, on a system that deliberately has none. Same split
# genlogo.py keeps: the tool lives in the port, the artefact lives in fs/.
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
PNG  = os.path.join(ROOT, "fs", "usr", "share", "kdos", "boot", "kdos-banner.png")
FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"

# What the boot screen says. THIS is the line to edit.
TITLE    = "KD's Homebrew Linux Distro"
SUBTITLE = "musl · toybox · kdos-comp · no systemd"

# Measured off the shipped banner, not chosen: the fitted DejaVu Sans Mono
# sizes reproduce the original ink widths to within 3px, the colours are the
# modal bright pixel of each line, and CENTRE is where both lines were
# already centred -- right of image centre, because the mascot sits left.
BG       = (0, 6, 2)
CENTRE   = 512
BAND     = (255, 198, 926, 260)          # x0, y0, x1, y1 -- captions only
TITLE_Y, TITLE_SZ, TITLE_FG    = 204, 22, (184, 255, 200)
SUB_Y,   SUB_SZ,   SUB_FG      = 235, 18, (255, 176, 0)

# The reference strings the Y positions were measured against. Ink top
# depends on which letters a string HAS -- "kdos-comp" has a descender the
# old text did not -- so both lines are placed by BASELINE, derived from
# these, rather than by their own ink box.
TITLE_REF = "KD's Homebrew Linux Distro"
SUB_REF   = "musl · toybox · cosmic · no systemd"


def origin_for(font, ref, ink_top):
    """The draw origin whose baseline matches what `ref` had at ink_top."""
    probe = Image.new("L", (1200, 120), 0)
    ImageDraw.Draw(probe).text((10, 40), ref, font=font, fill=255)
    return ink_top - (probe.getbbox()[1] - 40)


def stamp(im):
    d = ImageDraw.Draw(im)
    d.rectangle(BAND, fill=BG)
    for txt, ref, y, sz, fg in ((TITLE, TITLE_REF, TITLE_Y, TITLE_SZ, TITLE_FG),
                                (SUBTITLE, SUB_REF, SUB_Y, SUB_SZ, SUB_FG)):
        f = ImageFont.truetype(FONT, sz)
        d.text((CENTRE, origin_for(f, ref, y)), txt, font=f, fill=fg, anchor="ma")
    return im


def main():
    im = Image.open(PNG).convert("RGB")
    if "--check" in sys.argv:
        print("banner %dx%d" % im.size)
        print("  title    : %s" % TITLE)
        print("  subtitle : %s" % SUBTITLE)
        return
    w, h = im.size
    stamp(im).save(PNG)
    # The band must not have eaten anything else. A mascot or a rule inside
    # it would come back as flat background and nobody would notice until a
    # machine booted.
    chk = Image.open(PNG).convert("RGB")
    assert chk.size == (w, h), "banner changed size"
    print("%s: captions restamped" % PNG)
    print("  %s" % TITLE)
    print("  %s" % SUBTITLE)

main()
