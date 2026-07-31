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
# Lay out the KDOS GTK theme from the vendored adw-gtk3 stylesheet in theme/.
# Build step — no network, no SASS, no toolchain beyond python3.
#
#   gengtk.py <out-dir> [p dim sec urg deep text var pdark backdrop]
#
# The palette defaults to phosphor and takes the same nine colours, in the same
# order, as `palette()` in /usr/local/bin/kdos.
#
# Two passes, and the split matters:
#
#   1. Every literal colour in the stylesheet is hue-mapped by FAMILY — blues,
#      greens and purples to the accent hue, yellows/oranges/browns to the
#      secondary, reds to the urgent one, greys to a faintly tinted neutral.
#      Lightness is left alone: adw-gtk3's contrast steps are tuned, and
#      re-deriving them produces a worse theme than keeping them. This pass is
#      the safety net — it catches the 45-colour GNOME ramp, the `--blue-3`
#      style custom properties in :root, and the stray hex in helper classes,
#      so nothing anywhere stays blue.
#
#   2. The ~40 semantic names (window/view/headerbar/sidebar/card/popover/
#      dialog/accent/destructive/success/warning/error) are then overwritten
#      outright. These are what a widget actually resolves, so this is where
#      the KDOS look is decided, and it is deliberately not left to a hue
#      rotation of somebody else's grey.
#
# Fill colours use the DIM accent, never the full-intensity one: a 100%-filled
# GIMP opacity slider painted #39ff14 is an unreadable neon block.

import colorsys
import os
import re
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, 'theme')

# p dim sec urg deep text var pdark backdrop — kdos palette() order.
PHOSPHOR = ['39ff14', '12401f', 'ffb000', 'ff3131', '000a03',
            'b8ffc8', '04120a', '1f8f0c', '02120a']

HEX_RE = re.compile(r'#([0-9a-fA-F]{6}|[0-9a-fA-F]{3})\b')
DEF_RE = re.compile(r'^@define-color\s+([A-Za-z0-9_]+)\s+.*?;\s*$', re.M)
VAR_RE = re.compile(r'--([a-z0-9-]+)\s*:\s*[^;]+;')


def rgb(h):
    return tuple(int(h[i:i + 2], 16) / 255.0 for i in (0, 2, 4))


def hexs(r, g, b):
    return '#%02x%02x%02x' % tuple(max(0, min(255, round(c * 255)))
                                   for c in (r, g, b))


def mix(a, b, t):
    ra, rb = rgb(a.lstrip('#')), rgb(b.lstrip('#'))
    return hexs(*(x + (y - x) * t for x, y in zip(ra, rb)))


class Palette:
    def __init__(self, cols):
        (self.p, self.dim, self.sec, self.urg, self.deep,
         self.text, self.var, self.pdark, self.backdrop) = cols
        self.h_acc = colorsys.rgb_to_hls(*rgb(self.p))[0]
        self.h_sec = colorsys.rgb_to_hls(*rgb(self.sec))[0]
        self.h_urg = colorsys.rgb_to_hls(*rgb(self.urg))[0]
        self.h_neu = colorsys.rgb_to_hls(*rgb(self.var))[0]

        # Surfaces, darkest first. adw's own steps (#1d1d20 view, #222226
        # window, #2e2e32 headerbar, #36363a popover) are grey and much
        # lighter than KDOS wants, so they are replaced rather than shifted.
        d, t = '#' + self.deep, '#' + self.text
        self.view = d
        self.window = '#' + self.var
        self.header = mix(d, t, 0.07)
        self.side_backdrop = mix(d, t, 0.04)
        self.dialog = mix(d, t, 0.11)
        self.thumb = mix(d, t, 0.13)
        self.lift = mix(d, t, 0.09)
        self.on_accent = mix('#ffffff', t, 0.35)

    def family(self, h, s):
        """Map a hue onto the KDOS family that owns it."""
        deg = h * 360.0
        if s < 0.08:
            return self.h_neu, min(0.18, s * 1.4 + 0.05)
        if deg < 20 or deg >= 330:
            return self.h_urg, s
        if deg < 70:
            return self.h_sec, s
        if deg < 180 or (200 <= deg < 340):
            return self.h_acc, s
        return self.h_acc, s * 0.75

    def shift(self, m):
        raw = m.group(1)
        if len(raw) == 3:
            raw = ''.join(c * 2 for c in raw)
        h, l, s = colorsys.rgb_to_hls(*rgb(raw))
        if l <= 0.01 or l >= 0.99:
            return m.group(0)
        nh, ns = self.family(h, s)
        return hexs(*colorsys.hls_to_rgb(nh, l, ns))

    def names(self):
        d, t, p = '#' + self.deep, '#' + self.text, '#' + self.p
        acc, sec, urg = '#' + self.pdark, '#' + self.sec, '#' + self.urg
        return {
            'accent_bg_color': acc,
            'accent_fg_color': self.on_accent,
            'accent_color': p,
            'success_bg_color': acc,
            'success_fg_color': self.on_accent,
            'success_color': p,
            'warning_bg_color': mix(sec, d, 0.25),
            'warning_fg_color': d,
            'warning_color': sec,
            'destructive_bg_color': mix(urg, d, 0.2),
            'destructive_fg_color': mix('#ffffff', urg, 0.15),
            'destructive_color': urg,
            'error_bg_color': mix(urg, d, 0.2),
            'error_fg_color': mix('#ffffff', urg, 0.15),
            'error_color': urg,

            'window_bg_color': self.window,
            'window_fg_color': t,
            'view_bg_color': self.view,
            'view_fg_color': t,
            'headerbar_bg_color': self.header,
            'headerbar_fg_color': t,
            'headerbar_border_color': t,
            'headerbar_backdrop_color': self.window,
            'sidebar_bg_color': self.header,
            'sidebar_fg_color': t,
            'sidebar_backdrop_color': self.side_backdrop,
            'secondary_sidebar_bg_color': self.header,
            'secondary_sidebar_fg_color': t,
            'secondary_sidebar_backdrop_color': self.side_backdrop,
            'card_bg_color': 'alpha(%s, 0.07)' % t,
            'card_fg_color': t,
            'dialog_bg_color': self.dialog,
            'dialog_fg_color': t,
            'popover_bg_color': self.dialog,
            'popover_fg_color': t,
            'thumbnail_bg_color': self.thumb,
            'thumbnail_fg_color': t,
            'panel_bg_color': d,
            'panel_fg_color': t,

            'theme_bg_color': self.window,
            'theme_fg_color': t,
            'theme_base_color': self.view,
            'theme_text_color': t,
            'theme_selected_bg_color': acc,
            'theme_selected_fg_color': self.on_accent,
            'borders': mix(self.window, t, 0.22),
            'unfocused_borders': mix(self.window, t, 0.14),
            'wm_highlight': self.header,
            'content_view_bg': self.view,
            'text_view_bg': self.view,
        }


def convert(data, pal):
    data = HEX_RE.sub(pal.shift, data)
    over = pal.names()

    def redefine(m):
        name = m.group(1)
        return ('@define-color %s %s;' % (name, over[name])
                if name in over else m.group(0))

    data = DEF_RE.sub(redefine, data)

    # libadwaita's GTK 4.20 custom properties carry the same palette under
    # kebab-case names; a widget may resolve either spelling.
    def revar(m):
        name = m.group(1).replace('-', '_')
        return ('--%s: %s;' % (m.group(1), over[name])
                if name in over else m.group(0))

    return VAR_RE.sub(revar, data)


INDEX = """[Desktop Entry]
Type=X-GNOME-Metatheme
Name=KDOS
Comment=KDOS phosphor theme (adw-gtk3, recoloured)
Encoding=UTF-8

[X-GNOME-Metatheme]
GtkTheme=KDOS
MetacityTheme=KDOS
IconTheme=KDOS
CursorTheme=KDOS-cursors
ButtonLayout=:close
"""


def main():
    if len(sys.argv) < 2:
        sys.exit('usage: gengtk.py <out-dir> [9 palette colours]')
    out = sys.argv[1]
    cols = sys.argv[2:11] if len(sys.argv) >= 11 else PHOSPHOR
    pal = Palette(cols)

    shutil.rmtree(out, ignore_errors=True)
    for sub in ('gtk-3.0', 'gtk-4.0'):
        assets = os.path.join(SRC, sub, 'assets')
        if os.path.isdir(assets):
            shutil.copytree(assets, os.path.join(out, sub, 'assets'))

    n = 0
    for sub, name, aliases in (
            # GTK3 picks gtk.css normally and gtk-dark.css when the app asks
            # for dark; upstream's dark variant ships the two byte-identical,
            # and KDOS has no light mode, so one source fills both names.
            ('gtk-3.0', 'gtk-dark.css', ['gtk.css', 'gtk-dark.css']),
            ('gtk-4.0', 'gtk.css', ['gtk.css', 'gtk-dark.css']),
            ('gtk-4.0', 'libadwaita.css', ['libadwaita.css']),
            ('gtk-4.0', 'libadwaita-tweaks.css', ['libadwaita-tweaks.css'])):
        data = open(os.path.join(SRC, sub, name), encoding='utf-8').read()
        data = convert(data, pal)
        for alias in aliases:
            dst = os.path.join(out, sub, alias)
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            open(dst, 'w', encoding='utf-8').write(data)
            n += 1

    open(os.path.join(out, 'index.theme'), 'w').write(INDEX)
    print('kdos-gtk-theme: %d stylesheets -> %s' % (n, out))


main()
