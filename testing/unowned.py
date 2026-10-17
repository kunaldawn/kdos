#!/usr/bin/env python3
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   unowned — files in a built root that no package and no fs/ path provides
# ---------------------------------
#
#   python3 testing/unowned.py [--root build/fs] [--fs fs] [--all]
#
# A file nothing owns is one no removal takes away and no update replaces: a
# build that wrote outside its package root, a tool run against the live root,
# or a package whose manifest lost the line. Every regular file and symlink of
# the root is compared, keyed as kp_canon_path keys it, against the union of
# the package manifests in var/lib/kpkg/db and the paths fs/ provides (its
# tree and the fs-manifest the bootstrap sync records).
#
# Subtracted, because something other than a package writes them on purpose:
#   - the mount points and scratch a build uses: proc, sys, dev, run, tmp,
#     var/tmp, the chroot's /kdos and /ports binds, kpkg's caches;
#   - the homes and their template: home/, root/, etc/skel;
#   - kpkg's own state: var/lib/kpkg;
#   - the merged-/usr links and the var/run and var/lock links the
#     bootstrap makes;
#   - kpkg and kinstall, which 10_bootstrap compiles into the root and no
#     port packages (BOOTSTRAP below), and the passwd-, group-, shadow- and
#     gshadow- copies shadow's tools write beside the account files they
#     edit;
#   - what is generated over every package (GENERATED below): the shared
#     indexes kpkg's triggers and 70_image rebuild, depmod's module indexes,
#     the CA bundle update-ca-certificates writes, the launcher table
#     genlaunchers writes, the source offer 105_ports.sh writes and the
#     initramfs and its module list 090_initramfs.sh writes into boot/;
#   - Python bytecode under __pycache__: a package owns what kpkg compiled
#     into it, and what the build's own tools wrote is unowned until
#     020_cleanup.sh deletes it, so a run on a tree before that step would
#     list it. 020_cleanup.sh fails the image on a packaged module with no
#     bytecode.
#
# Grouped by top directories, largest group first; --all lists every path.
# Exit status 1 when anything is reported, 0 otherwise. 70_image runs it as a
# report (087_gates.sh) and does not stop on its status.

import argparse
import fnmatch
import os
import stat
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import treescan  # noqa: E402

PRUNE = ('proc', 'sys', 'dev', 'run', 'tmp', 'kdos', 'ports', 'home', 'root',
         'mnt', 'media', 'var/tmp', 'var/lib/kpkg', 'var/cache/kpkg',
         'var/cache/fontconfig', 'var/cache/man', 'etc/skel', 'lost+found')

# Written by kpkg's triggers (triggers.c) and by 70_image over every package,
# and the initramfs 70_image builds.
GENERATED = (
    'etc/ssl/cert.pem',
    'usr/lib/modules/*/modules.*',
    'usr/lib/fontconfig/cache/*',
    'etc/ld.so.cache',
    'etc/udev/hwdb.bin',
    'usr/share/info/dir',
    'usr/share/man/mandoc.db',
    'usr/share/glib-2.0/schemas/gschemas.compiled',
    'usr/lib/gio/modules/giomodule.cache',
    'usr/lib/gdk-pixbuf-2.0/*/loaders.cache',
    'usr/share/applications/mimeinfo.cache',
    'usr/share/icons/*/icon-theme.cache',
    'usr/share/fonts/*/fonts.dir',
    'usr/share/fonts/*/fonts.scale',
    'usr/share/fonts/*/*/fonts.dir',
    'usr/share/fonts/*/*/fonts.scale',
    'usr/share/texmf-dist/ls-R',
    'usr/share/texmf-local/ls-R',
    'usr/share/kdos/alien-apps',
    'usr/share/kdos/SOURCES',
    'var/lib/kdos/fs-manifest',
    'boot/initramfs.cpio.gz',
    'boot/initramfs.modules',
)
# What 10_bootstrap installs outside any package (120_kpkg.sh,
# 130_kinstall.sh), and the backups shadow's account tools keep.
BOOTSTRAP = (
    'usr/bin/kpkg',
    'usr/bin/kpkgadd',
    'usr/bin/kpkgbuild',
    'usr/bin/kpkgdel',
    'usr/bin/kpkgdepends',
    'etc/kpkg.conf',
    'usr/bin/kinstall',
    'etc/passwd-',
    'etc/group-',
    'etc/shadow-',
    'etc/gshadow-',
)
# The skeleton links 10_bootstrap/000_file_system.sh makes.
SKELETON = ('var/run', 'var/lock')
# update-mime-database writes the whole database beside packages/.
MIME_DB = 'usr/share/mime/'
MIME_SRC = 'usr/share/mime/packages/'


def fs_paths(fsdir, root):
    out = set()
    if os.path.isdir(fsdir):
        for rel, st in treescan.walk(fsdir):
            if not stat.S_ISDIR(st.st_mode):
                out.add(rel)
    try:
        with open(os.path.join(root, 'var/lib/kdos/fs-manifest'), encoding='utf-8',
                  errors='surrogateescape') as f:
            out.update(ln.strip() for ln in f if ln.strip())
    except OSError:
        pass
    return out


def generated(rel):
    if '/__pycache__/' in rel:
        return True
    if rel.startswith(MIME_DB) and not rel.startswith(MIME_SRC):
        return True
    return any(fnmatch.fnmatchcase(rel, g) for g in GENERATED)


def group(rel):
    parts = rel.split('/')
    n = 4 if parts[0] == 'usr' and len(parts) > 4 else 3
    return '/'.join(parts[:min(n, len(parts) - 1)]) or rel


def main():
    ap = argparse.ArgumentParser(description='unowned files in a built root')
    ap.add_argument('--root', default='build/fs')
    ap.add_argument('--fs', default='fs', help="the repository's fs/ tree")
    ap.add_argument('--all', action='store_true', help='list every unowned path')
    a = ap.parse_args()

    root = a.root
    if not os.path.isdir(os.path.join(root, 'var/lib/kpkg/db')):
        print(f'unowned: no package database under {root}', file=sys.stderr)
        return 2
    aliases = treescan.canon_load(root)
    _, owner = treescan.read_db(root, aliases)
    provided = {treescan.canon(aliases, p) for p in fs_paths(a.fs, root)}

    hits = []
    for rel, st in treescan.walk(root, '', PRUNE):
        if stat.S_ISDIR(st.st_mode):
            continue
        if rel in owner or rel in provided or generated(rel):
            continue
        if rel in treescan.ALIASES or rel in SKELETON or rel in BOOTSTRAP:
            continue
        hits.append((rel, st.st_size if stat.S_ISREG(st.st_mode) else 0))

    groups = {}
    for rel, size in hits:
        g = groups.setdefault(group(rel), [0, 0])
        g[0] += 1
        g[1] += size
    for g, (n, size) in sorted(groups.items(), key=lambda kv: (-kv[1][0], kv[0])):
        print(f'{n:>8} {size:>14}  {g}')
    if a.all:
        for rel, _ in sorted(hits):
            print('    ' + rel)
    print(f'unowned: {len(hits)} paths, {sum(s for _, s in hits)} bytes, '
          f'in {len(groups)} groups')
    return 1 if hits else 0


if __name__ == '__main__':
    sys.exit(main())
