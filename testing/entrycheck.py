#!/usr/bin/env python3
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   entrycheck — links, launchers and rules that point at nothing
# ---------------------------------
#
#   python3 testing/entrycheck.py [--root build/fs]
#
# Every reference below is resolved inside the root, an absolute link or path
# taken against the root and never the host.
#
#   link     A symlink that resolves to nothing. A target under /proc, /sys,
#            /dev, /run or /tmp is a run-time path and is not checked.
#   exec     A desktop entry's Exec= (applications, autostart and the skel
#            launchers) or a D-Bus service's Exec= (session and system
#            services) whose program is not an executable in the root: by
#            path when absolute, on PATH when bare. An entry whose TryExec=
#            does not resolve is one the menu hides by specification, and is
#            not checked further. The Start menu offers the first kind and
#            the bus activates the second; either one does nothing.
#   icon     A visible desktop entry (no NoDisplay=true, no Hidden=true)
#            whose Icon= names a file that does not exist, or a name no icon
#            theme under usr/share/icons and nothing in usr/share/pixmaps
#            provides. The menu draws it as a blank.
#   udev     A RUN, PROGRAM or IMPORT{program} key in a udev rule whose
#            program does not exist: by path when absolute, in usr/lib/udev
#            when bare. udev logs the failure at debug level only, so the
#            rule silently does nothing.
#
# Exit status 1 when anything is reported, 0 otherwise, 2 when the root
# cannot be read. 70_image runs it (087_gates.sh).

import argparse
import glob
import os
import re
import shlex
import stat
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import treescan  # noqa: E402

PRUNE = ('proc', 'sys', 'dev', 'run', 'tmp', 'kdos', 'ports', 'home', 'root',
         'var/tmp', 'var/cache/kpkg', 'lost+found')
RUNTIME = ('proc', 'sys', 'dev', 'run', 'tmp')
DESKTOP_DIRS = ('usr/share/applications', 'usr/local/share/applications',
                'etc/xdg/autostart', 'etc/skel/.local/share/applications')
DBUS_DIRS = ('usr/share/dbus-1/services', 'usr/share/dbus-1/system-services')
UDEV_DIRS = ('usr/lib/udev/rules.d', 'etc/udev/rules.d')
UDEV_LIB = 'usr/lib/udev'
ICON_EXT = ('.png', '.svg', '.svgz', '.xpm')
UDEV_KEY = re.compile(r'\b(RUN(?:\{program\})?|PROGRAM|IMPORT\{program\})\s*\+?=\s*"([^"]*)"')


def read_group(path, group):
    keys, cur = {}, None
    try:
        with open(path, encoding='utf-8', errors='replace') as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith('#'):
                    continue
                if line.startswith('['):
                    cur = line
                    continue
                if cur == group and '=' in line:
                    k, v = line.split('=', 1)
                    keys.setdefault(k.strip(), v.strip())
    except OSError:
        pass
    return keys


def program(cmdline):
    """The program an Exec= line runs: `env VAR=x` stripped, quotes undone."""
    try:
        args = shlex.split(cmdline.replace('%%', ''))
    except ValueError:
        args = cmdline.split()
    if args and os.path.basename(args[0]) == 'env':
        rest = args[1:]
        while rest and (rest[0].startswith('-') or '=' in rest[0]):
            rest = rest[1:]
        return rest[0] if rest else args[0]
    return args[0] if args else None


def resolves(root, prog):
    if not prog:
        return True
    if prog.startswith('/'):
        return treescan.is_exec_file(root, prog.lstrip('/'))
    return treescan.which(root, prog) is not None


def icon_index(root):
    names = set()
    for top in ('usr/share/icons', 'usr/share/pixmaps', 'usr/local/share/icons'):
        for rel, st in treescan.walk(root, top):
            if stat.S_ISDIR(st.st_mode):
                continue
            base = os.path.basename(rel)
            names.add(base)
            stem, ext = os.path.splitext(base)
            if ext in ICON_EXT:
                names.add(stem)
    return names


def main():
    ap = argparse.ArgumentParser(description='links, launchers and rules that point at nothing')
    ap.add_argument('--root', default='build/fs')
    a = ap.parse_args()
    root = a.root
    if not os.path.isdir(os.path.join(root, 'usr')):
        print(f'entrycheck: {root}/usr is not a directory', file=sys.stderr)
        return 2

    out = []
    count = {'link': 0, 'exec': 0, 'icon': 0, 'udev': 0}

    def report(kind, msg):
        count[kind] += 1
        out.append(f'{kind:<5} {msg}')

    for rel, st in treescan.walk(root, '', PRUNE):
        if not stat.S_ISLNK(st.st_mode):
            continue
        try:
            target = os.readlink(os.path.join(root, rel))
        except OSError:
            continue
        dest = treescan.resolve(root, rel, target)
        if dest is not None and dest.split('/', 1)[0] in RUNTIME:
            continue
        if treescan.lookup(root, rel) is None:
            report('link', f'{rel} -> {target}')

    icons = icon_index(root)

    def entries(dirs, pattern):
        for d in dirs:
            for p in sorted(glob.glob(os.path.join(root, d, '**', pattern), recursive=True)):
                yield os.path.relpath(p, root), p

    for rel, p in entries(DESKTOP_DIRS, '*.desktop'):
        e = read_group(p, '[Desktop Entry]')
        if e.get('Hidden', '').lower() == 'true':
            continue
        if e.get('Type', 'Application') != 'Application':
            continue
        if 'TryExec' in e and not resolves(root, e['TryExec']):
            continue
        prog = program(e.get('Exec', ''))
        if not resolves(root, prog):
            report('exec', f'{rel}: Exec={e.get("Exec")}')
        icon = e.get('Icon', '')
        if not icon or e.get('NoDisplay', '').lower() == 'true':
            continue
        if icon.startswith('/'):
            ok = treescan.exists(root, icon.lstrip('/'))
        else:
            ok = icon in icons
        if not ok:
            report('icon', f'{rel}: Icon={icon}')

    for rel, p in entries(DBUS_DIRS, '*.service'):
        e = read_group(p, '[D-BUS Service]')
        prog = program(e.get('Exec', ''))
        if not resolves(root, prog):
            report('exec', f'{rel}: Exec={e.get("Exec")}')

    for rel, p in entries(UDEV_DIRS, '*.rules'):
        try:
            with open(p, encoding='utf-8', errors='replace') as f:
                raw = f.read().split('\n')
        except OSError:
            continue
        lines, i = [], 0
        while i < len(raw):
            start, line = i + 1, raw[i]
            while line.endswith('\\') and i + 1 < len(raw):
                i += 1
                line = line[:-1] + ' ' + raw[i]
            lines.append((start, line))
            i += 1
        for n, line in lines:
            s = line.strip()
            if not s or s.startswith('#'):
                continue
            for key, val in UDEV_KEY.findall(s):
                try:
                    prog = (shlex.split(val) or [''])[0]
                except ValueError:
                    prog = val.split()[0] if val.split() else ''
                if not prog or prog[0] in '$%':
                    continue
                path = prog.lstrip('/') if prog.startswith('/') else UDEV_LIB + '/' + prog
                if not treescan.is_exec_file(root, path):
                    report('udev', f'{rel}:{n}: {key}="{val}"')

    for line in out:
        print(line)
    print('entrycheck: ' + ', '.join(f'{k} {v}' for k, v in count.items()))
    return 1 if out else 0


if __name__ == '__main__':
    sys.exit(main())
