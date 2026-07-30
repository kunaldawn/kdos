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
# Lay out the KDOS icon theme from the vendored Papirus artwork in art/.
# Build step — no network, no rasteriser, nothing but python3.
#
#   genicons.py <out-dir> [p dim sec urg deep text var pdark backdrop]
#
# The palette defaults to phosphor and takes the same nine colours, in the same
# order, as `palette()` in /usr/local/bin/kdos.
#
# Colours are mapped by FAMILY, not flattened: blues/greens/purples go to the
# accent hue, yellows/oranges/browns to the secondary, reds to the urgent one,
# and near-greys to a faintly tinted neutral — each keeping its own lightness.
# That is deliberate. Papirus colour-codes mimetypes (documents, archives,
# audio, video), and collapsing every hue onto one accent turns a folder full
# of files into a wall of identical green lozenges. Mapping families keeps the
# coding readable while making the whole set unmistakably phosphor.
#
# The same mapping runs in kdos-gtk-theme's gengtk.py, so icons and widgets
# agree about what "green" means.
#
# Symlinks are Papirus's alias mechanism and are recreated as symlinks; only
# real files are read and rewritten.

import colorsys
import os
import re
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ART = os.path.join(HERE, 'art')

PHOSPHOR = ['39ff14', '12401f', 'ffb000', 'ff3131', '000a03',
            'b8ffc8', '04120a', '1f8f0c', '02120a']

HEX_RE = re.compile(r'#([0-9a-fA-F]{6}|[0-9a-fA-F]{3})\b')

CONTEXTS = {
    'actions': 'Actions',
    'categories': 'Categories',
    'devices': 'Devices',
    'emblems': 'Emblems',
    'mimetypes': 'MimeTypes',
    'panel': 'Status',
    'places': 'Places',
    'status': 'Status',
}


def rgb(h):
    return tuple(int(h[i:i + 2], 16) / 255.0 for i in (0, 2, 4))


class Palette:
    def __init__(self, cols):
        (self.p, self.dim, self.sec, self.urg, self.deep,
         self.text, self.var, self.pdark, self.backdrop) = cols
        self.h_acc = colorsys.rgb_to_hls(*rgb(self.p))[0]
        self.h_sec = colorsys.rgb_to_hls(*rgb(self.sec))[0]
        self.h_urg = colorsys.rgb_to_hls(*rgb(self.urg))[0]
        self.h_neu = colorsys.rgb_to_hls(*rgb(self.var))[0]

    def family(self, h, s):
        deg = h * 360.0
        if s < 0.08:
            return self.h_neu, min(0.18, s * 1.4 + 0.05)
        if deg < 20 or deg >= 330:
            return self.h_urg, s
        if deg < 70:
            return self.h_sec, s
        if deg < 180 or 200 <= deg < 340:
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
        r, g, b = colorsys.hls_to_rgb(nh, l, ns)
        return '#%02x%02x%02x' % tuple(max(0, min(255, round(c * 255)))
                                       for c in (r, g, b))


def recolor_tree(out, pal):
    sizes, files, links = [], 0, 0
    for size in sorted(os.listdir(ART)):
        sd = os.path.join(ART, size)
        if not os.path.isdir(sd):
            continue
        for ctx in sorted(os.listdir(sd)):
            if ctx not in CONTEXTS:
                continue
            od = os.path.join(out, size, ctx)
            os.makedirs(od, exist_ok=True)
            sizes.append((size, ctx))
            for name in sorted(os.listdir(os.path.join(sd, ctx))):
                s = os.path.join(sd, ctx, name)
                d = os.path.join(od, name)
                if os.path.islink(s):
                    os.symlink(os.readlink(s), d)
                    links += 1
                    continue
                with open(s, encoding='utf-8') as f:
                    data = f.read()
                with open(d, 'w', encoding='utf-8') as f:
                    f.write(HEX_RE.sub(pal.shift, data))
                files += 1
    return sizes, files, links


def write_index(out, dirs):
    lines = ['[Icon Theme]',
             'Name=KDOS',
             'Comment=KDOS phosphor icon theme',
             # Cosmic still supplies the COSMIC applets' own icons and the
             # symbolic set the shell tints itself; hicolor supplies the alien
             # apps' icons, installed there by 06_packaging/01_appbox.sh.
             'Inherits=Cosmic,Pop,hicolor',
             'Directories=' + ','.join(d for d, _, _ in dirs),
             '']
    for path, ctx, size in dirs:
        lines += ['[%s]' % path, 'Context=%s' % ctx]
        if size:
            lines += ['Size=%d' % size, 'Type=Fixed']
        else:
            lines += ['Size=64', 'MinSize=8', 'MaxSize=512', 'Type=Scalable']
        lines.append('')
    with open(os.path.join(out, 'index.theme'), 'w') as f:
        f.write('\n'.join(lines))


def main():
    if len(sys.argv) < 2:
        sys.exit('usage: genicons.py <out-dir> [9 palette colours]')
    out = sys.argv[1]
    pal = Palette(sys.argv[2:11] if len(sys.argv) >= 11 else PHOSPHOR)

    shutil.rmtree(out, ignore_errors=True)
    os.makedirs(out, exist_ok=True)
    sized, files, links = recolor_tree(out, pal)

    dirs = [('%s/%s' % (size, ctx), CONTEXTS[ctx], int(size.split('x')[0]))
            for size, ctx in sized]
    # KDOS's own marks live outside the vendored tree.
    dirs.append(('scalable/apps', 'Applications', 0))
    dirs.append(('256x256/apps', 'Applications', 256))
    write_index(out, dirs)
    print('kdos-icons: %d icons, %d aliases, %d directories -> %s'
          % (files, links, len(dirs), out))


main()
