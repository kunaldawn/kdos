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
# Re-vendor theme/ from an upstream adw-gtk3 release.
#
# MAINTENANCE TOOL, not part of the build: it runs on the host, with network,
# when the stylesheet should be refreshed. The build only ever reads the pruned
# theme/ committed next to it.
#
#   vendor.py <adw-gtk3vX.Y.tar.xz>
#
# Why adw-gtk3 and not Adwaita, Colloid, Orchis or Graphite: it is the only one
# of them whose whole stylesheet is written against GTK NAMED COLORS. Stock
# Adwaita 3.24 is compiled from SASS with literal hex baked into every rule, so
# redefining theme_bg_color reaches the handful of widgets that reference the
# name and leaves the rest grey — that is exactly what "themes still not
# matching" looked like in GIMP. adw-gtk3 has ~125 @define-color at the top and
# almost nothing below, so the KDOS palette is a header rewrite (kdos-theme) and
# every widget follows. It is also the libadwaita stylesheet, which means GTK3
# apps and GTK4/libadwaita apps in the appbox end up genuinely identical rather
# than merely similar.
#
# What it throws away:
#   * The light variant. KDOS is dark-only.
#   * gtk-3.0/gtk.css, which is byte-identical to gtk-dark.css in the dark
#     variant — the build writes both names from the one file.
#   * thumbnail.png (theme-picker preview; nothing on KDOS shows one).
#   * Every asset the stylesheet does not actually reference. Upstream ships
#     both variants' scale-mark sliders and text handles.
#
# Assets are NOT recoloured, here or at build time, and that is a finding
# rather than an omission: every one of them is neutral grey (210,210,210) or a
# symbolic mask that the toolkit tints from the active palette. Nothing in
# assets/ carries the upstream accent blue.

import os
import re
import shutil
import sys
import tarfile
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
THEME = os.path.join(HERE, 'theme')

VARIANT = 'adw-gtk3-dark'
CSS = [
    ('gtk-3.0', 'gtk-dark.css'),
    ('gtk-4.0', 'libadwaita.css'),
    ('gtk-4.0', 'libadwaita-tweaks.css'),
    ('gtk-4.0', 'gtk.css'),
]
ASSET_RE = re.compile(r'assets/([A-Za-z0-9@._-]+)')


def main():
    if len(sys.argv) != 2:
        sys.exit('usage: vendor.py <adw-gtk3vX.Y.tar.xz>')
    src = sys.argv[1]

    with tempfile.TemporaryDirectory() as tmp:
        with tarfile.open(src) as tf:
            tf.extractall(tmp)
        root = os.path.join(tmp, VARIANT)
        if not os.path.isdir(root):
            sys.exit('%s: no %s/ inside' % (src, VARIANT))

        shutil.rmtree(THEME, ignore_errors=True)
        wanted = {'gtk-3.0': set(), 'gtk-4.0': set()}
        for sub, name in CSS:
            data = open(os.path.join(root, sub, name), encoding='utf-8').read()
            wanted[sub] |= set(ASSET_RE.findall(data))
            dst = os.path.join(THEME, sub, name)
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            open(dst, 'w', encoding='utf-8').write(data)

        kept = 0
        for sub, names in wanted.items():
            for name in sorted(names):
                a = os.path.join(root, sub, 'assets', name)
                if not os.path.isfile(a):
                    continue
                b = os.path.join(THEME, sub, 'assets', name)
                os.makedirs(os.path.dirname(b), exist_ok=True)
                shutil.copy2(a, b)
                kept += 1

    ver = re.search(r'adw-gtk3v([0-9.]+)', os.path.basename(src))
    with open(os.path.join(THEME, 'UPSTREAM'), 'w') as f:
        f.write('adw-gtk3 %s (dark variant only)\n'
                'https://github.com/lassekongo83/adw-gtk3\n'
                'LGPL-2.1 — see ../LICENSE.notice\n'
                % (ver.group(1) if ver else '?'))

    total = sum(os.path.getsize(os.path.join(r, n))
                for r, _, fs in os.walk(THEME) for n in fs)
    print('kdos-gtk-theme: %d css + %d assets, %.1f MiB -> %s'
          % (len(CSS), kept, total / 1048576.0, THEME))


main()
