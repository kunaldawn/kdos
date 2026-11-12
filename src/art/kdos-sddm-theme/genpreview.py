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
# The KDOS greeter's preview.png, the picture System Settings → Login Screen
# shows for the theme: a schematic of the greeter in one accent's greeter
# set, drawn from that set and nothing else.
#
#   genpreview.py SET OUT
#
# SET is /usr/share/kdos/greeter/<accent>: its background.png is the ground,
# and its kdeglobals gives every colour — the Complementary set the greeter
# draws its text in, the View set its password field — and the fixed-width
# family of the clock, falling back to the distribution's
# /usr/share/kdos/xdg/kdeglobals as the greeter's own cascade does. No colour
# is named here, so the preview follows the palette the greeter wears.
#
# THE LAYOUT IS THE GREETER'S, measured on a 1920x1080 screen and scaled to
# the output: the clock and date above, the user's circle and name in the
# middle, the password field under it and the credit at the bottom right, on
# the blurred background. The time and date are a fixed sample, not the
# build's clock, so two builds write the same file.

import subprocess
import sys

from PIL import Image, ImageDraw, ImageFilter, ImageFont

W, H = 1280, 720
S = H / 1080  # the greeter's geometry below is measured on a 1920x1080 screen
TIME = "09:41"
DATE = "Monday, 1 January 2029"
NAME = "User"
CREDIT = "KDOS · Plasma by KDE"
FALLBACK = "/usr/share/kdos/xdg/kdeglobals"


def read_ini(path):
    groups, cur = {}, None
    try:
        with open(path, encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if not line or line[0] in "#;":
                    continue
                if line.startswith("[") and line.endswith("]"):
                    cur = groups.setdefault(line[1:-1], {})
                elif cur is not None and "=" in line:
                    k, v = line.split("=", 1)
                    cur[k.strip()] = v.strip()
    except OSError:
        pass
    return groups


def lookup(cfgs, group, key):
    for c in cfgs:
        v = c.get(group, {}).get(key)
        if v:
            return v
    sys.exit(f"genpreview: no [{group}] {key} in the greeter set or {FALLBACK}")


def colour(cfgs, group, key, alpha=255):
    r, g, b = (int(x) for x in lookup(cfgs, group, key).split(",")[:3])
    return (r, g, b, alpha)


def face(family, weight=None):
    pattern = family + (f":weight={weight}" if weight else "")
    out = subprocess.run(["fc-match", "--format=%{file}", pattern],
                         capture_output=True, text=True, check=True).stdout
    if not out:
        sys.exit(f"genpreview: fontconfig has no face for '{pattern}'")
    return out


def main():
    if len(sys.argv) != 3:
        sys.exit("usage: genpreview.py SET OUT")
    sset, out = sys.argv[1], sys.argv[2]
    cfgs = [read_ini(f"{sset}/kdeglobals"), read_ini(FALLBACK)]

    fg = colour(cfgs, "Colors:Complementary", "ForegroundNormal")
    dim = colour(cfgs, "Colors:Complementary", "ForegroundInactive")
    plate = colour(cfgs, "Colors:Complementary", "BackgroundNormal", 220)
    field = colour(cfgs, "Colors:View", "BackgroundNormal", 230)
    edge = colour(cfgs, "Colors:View", "DecorationFocus")
    mono = lookup(cfgs, "General", "fixed").split(",")[0]
    sans = lookup(cfgs, "General", "font").split(",")[0]

    # The background is cropped to fill, as Background.qml's
    # PreserveAspectCrop, and blurred, as Breeze blurs it under the login.
    bg = Image.open(f"{sset}/background.png").convert("RGB")
    k = max(W / bg.width, H / bg.height)
    bg = bg.resize((round(bg.width * k), round(bg.height * k)), Image.LANCZOS)
    x0, y0 = (bg.width - W) // 2, (bg.height - H) // 2
    bg = bg.crop((x0, y0, x0 + W, y0 + H)).filter(ImageFilter.GaussianBlur(48 * S))
    img = bg.convert("RGBA")

    over = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(over)
    px = lambda v: round(v * S)
    cx = W // 2

    d.text((cx, px(178)), TIME, fill=fg, anchor="mm",
           font=ImageFont.truetype(face(mono, "semibold"), px(96)))
    d.text((cx, px(268)), DATE, fill=fg, anchor="mm",
           font=ImageFont.truetype(face(mono), px(32)))

    # The user's picture: Breeze's default face, a head and shoulders in a
    # ring, the shoulders cut by the ring.
    cy, r, ring = px(467), px(62), max(2, px(3))
    d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=plate)
    glyph = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    g = ImageDraw.Draw(glyph)
    hr = px(19)
    g.ellipse((cx - hr, cy - px(44), cx + hr, cy - px(44) + 2 * hr),
              outline=fg, width=ring)
    g.ellipse((cx - px(44), cy + px(8), cx + px(44), cy + px(96)),
              outline=fg, width=ring)
    mask = Image.new("L", (W, H), 0)
    ImageDraw.Draw(mask).ellipse((cx - r, cy - r, cx + r, cy + r), fill=255)
    over.paste(glyph, (0, 0), Image.composite(glyph, Image.new("RGBA", (W, H)), mask))
    d.ellipse((cx - r, cy - r, cx + r, cy + r), outline=fg, width=ring)
    d.text((cx, px(563)), NAME, fill=fg, anchor="mm", stroke_width=max(1, px(2)),
           stroke_fill=plate, font=ImageFont.truetype(face(sans), px(21)))

    d.rounded_rectangle((px(816), px(603), px(1066), px(634)), radius=px(3),
                        fill=field, outline=edge, width=max(1, px(1.5)))
    d.rounded_rectangle((px(1072), px(603), px(1103), px(634)), radius=px(3),
                        fill=plate, outline=dim, width=max(1, px(1)))

    d.text((W - px(12), H - px(12)), CREDIT, fill=fg, anchor="rb",
           font=ImageFont.truetype(face(sans), px(14)))

    Image.alpha_composite(img, over).convert("RGB").save(out, optimize=True)


if __name__ == "__main__":
    main()
