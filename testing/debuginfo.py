#!/usr/bin/env python3
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   debuginfo — the DWARF a built tree ships, and the gate on it
# ---------------------------------
#
#   python3 testing/debuginfo.py [--root build/fs] [--threshold BYTES] [--files]
#
# kpkg strips nothing: a port ships debug information exactly when its compile
# or link flags produce it, and no port is meant to. One walk over usr/ of the
# root reads every regular file's first four bytes; each ELF file's section
# table is parsed in place and every `.debug_*` and `.zdebug*` section that
# occupies bytes is summed. The totals are grouped by the package whose
# manifest in var/lib/kpkg/db names the file, keyed as kp_canon_path keys it,
# largest first. A file no manifest names is listed under (unowned).
#
# Left out, because their debug information is not a flag a recipe sets or
# they are not programs this system runs: kernel modules, firmware, the
# cross-target sysroots (arm-none-eabi, avr, riscv*-elf) and BPF objects.
#
# ALLOWED: ghc's `libHSrts*_debug-ghc*.so`. They are the runtimes `ghc -debug`
# links, and carrying DWARF is what they are for; stripping them leaves the
# flag producing programs that cannot be debugged. They are counted and shown
# on their own line, never in the total the threshold reads.
#
# Exit status 1 when the DWARF outside the allowlist exceeds the threshold
# (default 1 MiB), 0 otherwise, 2 when the root cannot be read.

import argparse
import fnmatch
import os
import stat
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import treescan  # noqa: E402

PRUNE = ('usr/lib/modules',)
SKIP_PARTS = ('firmware', 'arm-none-eabi', 'avr')
ALLOW = ('libHSrts*_debug-ghc*.so',)


def skipped(rel):
    parts = rel.split('/')
    for p in parts[:-1]:
        if p in SKIP_PARTS or fnmatch.fnmatch(p, 'riscv*-elf'):
            return True
    return rel.endswith('.bpf.o')


def dwarf_bytes(elf):
    return sum(size for name, ty, size in elf.sections
               if ty != treescan.SHT_NOBITS
               and (name.startswith('.debug_') or name.startswith('.zdebug')))


def main():
    ap = argparse.ArgumentParser(description='DWARF shipped in a built tree')
    ap.add_argument('--root', default='build/fs')
    ap.add_argument('--threshold', type=int, default=1 << 20,
                    help='bytes of DWARF outside the allowlist that fail the gate')
    ap.add_argument('--files', action='store_true', help='list every file under its package')
    a = ap.parse_args()

    root = a.root
    if not os.path.isdir(os.path.join(root, 'usr')):
        print(f'debuginfo: {root}/usr is not a directory', file=sys.stderr)
        return 2
    aliases = treescan.canon_load(root)
    _, owner = treescan.read_db(root, aliases)

    pkgs, allowed = {}, []
    total = nfiles = 0
    for rel, st in treescan.walk(root, 'usr', PRUNE):
        if not stat.S_ISREG(st.st_mode) or st.st_size < 64 or skipped(rel):
            continue
        full = os.path.join(root, rel)
        if not treescan.is_elf(full):
            continue
        n = dwarf_bytes(treescan.Elf(full))
        if not n:
            continue
        if any(fnmatch.fnmatch(os.path.basename(rel), g) for g in ALLOW):
            allowed.append((n, rel))
            continue
        p = owner.get(rel, '(unowned)')
        e = pkgs.setdefault(p, [0, []])
        e[0] += n
        e[1].append((n, rel))
        total += n
        nfiles += 1

    for p, (n, fl) in sorted(pkgs.items(), key=lambda kv: -kv[1][0]):
        print(f'{n:>14}  {p}  ({len(fl)} files)')
        if a.files:
            for fn, rel in sorted(fl, reverse=True):
                print(f'    {fn:>12}  {rel}')
    if allowed:
        print(f'allowed: {len(allowed)} ghc debug runtime(s), '
              f'{sum(n for n, _ in allowed)} bytes of DWARF')
    print(f'debuginfo: {nfiles} files in {len(pkgs)} packages carry '
          f'{total} bytes of DWARF; threshold {a.threshold}')
    return 1 if total > a.threshold else 0


if __name__ == '__main__':
    sys.exit(main())
