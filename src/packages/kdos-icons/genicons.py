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

# Papirus's apps/ is deliberately not vendored (see vendor.py), so without this
# the theme has no Applications context at all and every app icon on the
# desktop — including COSMIC's own — comes from Cosmic or hicolor untinted,
# which is what "the KDOS icon theme is not being used" looks like. These two
# directories are recoloured into scalable/apps at generation time:
#
#   Cosmic/scalable/apps      COSMIC's stock application icons
#   hicolor/scalable/apps     but ONLY com.system76.* — 06_packaging installs
#                             the ALIEN apps' icons into that same directory,
#                             and a phosphor Firefox logo is vandalism, not
#                             theming.
#
# Symbolic icons are skipped throughout: the toolkit tints those from the
# active COSMIC palette already, and recolouring them fixes them at one accent.
# Both are swept across EVERY size directory, not just scalable/: COSMIC's own
# app icons are SVGs filed under fixed sizes (com.system76.CosmicFiles lives at
# 24x24, 128x128 and 256x256 and nowhere else), which is why the dock's Files
# and Settings buttons stayed stock when only scalable/ was read. The largest
# variant of each name wins and is written to scalable/apps, since they are all
# SVG anyway.
APP_SOURCES = (
    ('/usr/share/icons/Cosmic', None),
    ('/usr/share/icons/hicolor', 'com.system76.'),
)

# KDOS's own marks, which are not in the vendored artwork. Kept here rather
# than in the kpkgbuild so that `kdos theme <accent>` — which re-runs this
# script against $HOME — produces a complete theme on its own.
#
# They are PNG at every size rather than one SVG: COSMIC's SVG renderer (resvg)
# is built without raster-image support, so an SVG wrapping the artwork renders
# empty, and the <rect> pixel art that used to work around that was a 34x34
# grid blown up to 256. marks/ is cut from kdos.png by genmarks.py.
MARKS = os.path.join(HERE, 'marks')
MARK_SETS = (
    # The dock's app-library button shows the TARGET's icon
    # (com.system76.CosmicAppLibrary), not the applet's own, so both names
    # have to become the tux.
    ('tux', ('com.system76.CosmicAppLibrary', 'com.system76.CosmicPanelAppButton')),
    ('logo', ('distributor-logo-kdos', 'start-here')),
)

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


def recolor_apps(out, pal):
    """Recolour COSMIC's application icons into the theme's own apps context."""
    best = {}
    for root, prefix in APP_SOURCES:
        if not os.path.isdir(root):
            continue
        for size in sorted(os.listdir(root)):
            d = os.path.join(root, size, 'apps')
            if not os.path.isdir(d):
                continue
            rank = 1 << 20 if size == 'scalable' else 0
            try:
                rank = rank or int(size.split('x')[0])
            except ValueError:
                continue
            for name in sorted(os.listdir(d)):
                if not name.endswith('.svg') or name.endswith('-symbolic.svg'):
                    continue
                if prefix and not name.startswith(prefix):
                    continue
                if best.get(name, (-1,))[0] < rank:
                    best[name] = (rank, os.path.join(d, name))

    od = os.path.join(out, 'scalable', 'apps')
    os.makedirs(od, exist_ok=True)
    for name, (_, src) in best.items():
        with open(src, encoding='utf-8') as f:
            data = f.read()
        with open(os.path.join(od, name), 'w', encoding='utf-8') as f:
            f.write(HEX_RE.sub(pal.shift, data))
    return len(best)


def install_marks(out):
    n = 0
    if not os.path.isdir(MARKS):
        return n
    for stem, names in MARK_SETS:
        for f in sorted(os.listdir(MARKS)):
            if not f.startswith(stem + '-') or not f.endswith('.png'):
                continue
            size = f[len(stem) + 1:-4]
            if not size.isdigit():
                continue
            od = os.path.join(out, '%sx%s' % (size, size), 'apps')
            os.makedirs(od, exist_ok=True)
            for name in names:
                shutil.copyfile(os.path.join(MARKS, f),
                                os.path.join(od, name + '.png'))
                n += 1
    # recolor_apps just wrote COSMIC's own SVG under these names, and a
    # scalable icon beats a fixed-size PNG at any requested size — so the
    # stock grid button would win over the tux. Drop them.
    for _, names in MARK_SETS:
        for name in names:
            p = os.path.join(out, 'scalable', 'apps', name + '.svg')
            if os.path.exists(p):
                os.unlink(p)
    return n


def write_index(out):
    """Directories are read back off the generated tree rather than tracked by
    hand: the marks alone create eight <size>/apps directories, and an icon in
    a directory index.theme does not list is invisible."""
    dirs = []
    for entry in sorted(os.listdir(out)):
        ed = os.path.join(out, entry)
        if not os.path.isdir(ed):
            continue
        for ctx in sorted(os.listdir(ed)):
            if not os.path.isdir(os.path.join(ed, ctx)):
                continue
            dirs.append(('%s/%s' % (entry, ctx),
                         'Applications' if ctx == 'apps' else CONTEXTS.get(ctx, 'Actions'),
                         0 if entry == 'scalable' else int(entry.split('x')[0])))

    lines = ['[Icon Theme]',
             'Name=KDOS',
             'Comment=KDOS phosphor icon theme',
             # Cosmic still supplies the symbolic set the shell tints itself;
             # hicolor supplies the alien apps' icons, installed there by
             # 06_packaging/01_appbox.sh.
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
    return len(dirs)


def main():
    if len(sys.argv) < 2:
        sys.exit('usage: genicons.py <out-dir> [9 palette colours]')
    out = sys.argv[1]
    pal = Palette(sys.argv[2:11] if len(sys.argv) >= 11 else PHOSPHOR)

    shutil.rmtree(out, ignore_errors=True)
    os.makedirs(out, exist_ok=True)
    sized, files, links = recolor_tree(out, pal)

    apps = recolor_apps(out, pal)
    marks = install_marks(out)         # after recolor_apps: the tux wins
    ndirs = write_index(out)
    print('kdos-icons: %d icons, %d aliases, %d apps, %d marks, %d directories -> %s'
          % (files, links, apps, marks, ndirs, out))


main()
