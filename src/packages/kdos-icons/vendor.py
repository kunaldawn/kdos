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
# Re-vendor art/ from an upstream Papirus release.
#
# MAINTENANCE TOOL, not part of the build: it runs on the host, with network,
# when the artwork should be refreshed. The build only ever reads the pruned
# art/ committed next to it.
#
#   vendor.py <papirus-icon-theme-YYYYMMDD.tar.gz>
#
# Papirus over Tela/Colloid/Qogir for one reason that matters here: coverage.
# KDOS's desktop is COSMIC plus ~90 Debian apps in a container, and Papirus is
# the only free set that has a real icon for essentially every mimetype, device
# and place either of them will ask for. It is also flat single-fill SVG, so a
# palette remap is a colour substitution rather than a redraw.
#
# What it throws away, and why:
#   * apps/. The alien apps ship their own icons and 01_appbox.sh installs them
#     into hicolor; overriding Firefox and GIMP with somebody's redraw makes the
#     launcher HARDER to read, not easier. apps/ is also 76 MB of the 90.
#   * All but six sizes. Upstream ships 8..128 plus every @2x alias.
#   * Every folder/user colour variant except blue, which is upstream's DEFAULT
#     (folder-cd.svg is a symlink to folder-blue-cd.svg) and becomes phosphor
#     after the recolour anyway. Twenty-one other colourways is 11 MB of
#     artwork nothing on KDOS can select.
#   * Papirus-Dark is merged over Papirus rather than kept as a second theme:
#     it only really differs at 16/22/24, everything else is a symlink back,
#     and KDOS has no light mode.
#
# Symlinks are preserved — they are how Papirus expresses icon aliases, and
# resolving them would multiply the tree. Any link left dangling by the pruning
# is dropped instead, iteratively (links can point at links).

import os
import re
import shutil
import sys
import tarfile
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ART = os.path.join(HERE, 'art')

SIZES = ('16x16', '22x22', '24x24', '32x32', '48x48', '64x64')
CONTEXTS = ('actions', 'categories', 'devices', 'emblems', 'mimetypes',
            'panel', 'places', 'status')
# Upstream's default colourway is blue; every other one is dropped.
DROP = re.compile(r'^(folder|user)-(black|bluegrey|breeze|brown|carmine|cyan|'
                  r'darkcyan|deeporange|green|grey|indigo|magenta|nordic|'
                  r'orange|palebrown|paleorange|pink|red|teal|violet|white|'
                  r'yellow)(-|\.)')


def main():
    if len(sys.argv) != 2:
        sys.exit('usage: vendor.py <papirus-icon-theme-YYYYMMDD.tar.gz>')
    src = sys.argv[1]

    with tempfile.TemporaryDirectory() as tmp:
        with tarfile.open(src) as tf:
            tf.extractall(tmp)
        roots = [d for d in os.listdir(tmp) if d.startswith('papirus-icon-theme')]
        if not roots:
            sys.exit('%s: no papirus-icon-theme-* inside' % src)
        root = os.path.join(tmp, roots[0])

        shutil.rmtree(ART, ignore_errors=True)
        real = links = 0
        # Papirus first, Papirus-Dark second so the dark overrides land on top.
        for theme in ('Papirus', 'Papirus-Dark'):
            for size in SIZES:
                sd = os.path.join(root, theme, size)
                if not os.path.isdir(sd) or os.path.islink(sd):
                    continue
                for ctx in CONTEXTS:
                    cd = os.path.join(sd, ctx)
                    if not os.path.isdir(cd) or os.path.islink(cd):
                        continue
                    out = os.path.join(ART, size, ctx)
                    os.makedirs(out, exist_ok=True)
                    for name in sorted(os.listdir(cd)):
                        if DROP.match(name):
                            continue
                        s, d = os.path.join(cd, name), os.path.join(out, name)
                        if os.path.islink(s):
                            if os.path.lexists(d):
                                os.unlink(d)
                            os.symlink(os.readlink(s), d)
                            links += 1
                        else:
                            shutil.copy2(s, d)
                            real += 1

        dropped = prune_dangling()
        shutil.copy2(os.path.join(root, 'LICENSE'),
                     os.path.join(ART, 'LICENSE.upstream'))
        ver = re.search(r'(\d{8})', roots[0])
        with open(os.path.join(ART, 'UPSTREAM'), 'w') as f:
            f.write('papirus-icon-theme %s (Papirus + Papirus-Dark, merged)\n'
                    'https://github.com/PapirusDevelopmentTeam/papirus-icon-theme\n'
                    'GPL-3.0 — see ../LICENSE.notice\n'
                    % (ver.group(1) if ver else '?'))

    total = sum(os.path.getsize(os.path.join(r, n))
                for r, _, fs in os.walk(ART) for n in fs
                if not os.path.islink(os.path.join(r, n)))
    print('kdos-icons: %d icons + %d aliases (%d dangling dropped), %.1f MiB -> %s'
          % (real, links - dropped, dropped, total / 1048576.0, ART))


def prune_dangling():
    """Drop aliases the pruning left pointing at nothing. Iterative: Papirus
    chains links through links, so one pass can strand the next."""
    dropped = 0
    while True:
        gone = 0
        for r, _, files in os.walk(ART):
            for n in files:
                p = os.path.join(r, n)
                if os.path.islink(p) and not os.path.exists(p):
                    os.unlink(p)
                    gone += 1
        dropped += gone
        if not gone:
            return dropped


main()
