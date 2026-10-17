#!/usr/bin/env python3
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   treescan — what the tree gates read off a built root
# ---------------------------------
#
# Shared by debuginfo.py, unowned.py, linkcheck.py and entrycheck.py. Not a
# program: it reads, it never writes.
#
# OWNERSHIP IS KEYED THE WAY KPKG KEYS IT. kp_canon_path folds a top-level
# merged-/usr alias (`bin`, `sbin`, `lib`, `lib64`, `lib32` linking to
# `usr/<same name>`) into its target, so `./lib/x` in a manifest and the walked
# file `usr/lib/x` are one key. A gate that compares the raw strings reports
# every file a package installed through an alias as unowned.
#
# ELF files are recognised by their first four bytes, read once per regular
# file in the single walk; nothing is spawned per file. Section and dynamic
# tables are parsed here for ELF32 and ELF64 of either byte order.

import os
import stat
import struct

ALIASES = ('bin', 'sbin', 'lib', 'lib64', 'lib32')
ELF_MAGIC = b'\x7fELF'
SHT_NOBITS = 8
PT_LOAD, PT_DYNAMIC, PT_INTERP = 1, 2, 3
DT_NEEDED, DT_STRTAB, DT_RPATH, DT_RUNPATH, DT_SONAME = 1, 5, 15, 29, 14


def canon_load(root):
    """[(alias, target)] for the root's merged-/usr links, as kp_canon_load."""
    out = []
    for n in ALIASES:
        try:
            t = os.readlink(os.path.join(root, n))
        except OSError:
            continue
        if t.lstrip('/') == 'usr/' + n:
            out.append((n, 'usr/' + n))
    return out


def tar_unquote(s):
    """A `tar -tf` name with GNU tar's quoting undone.

    The manifest is tar's listing, which writes a byte outside printable ASCII
    as `\\ooo` and a backslash as `\\\\`; the walked name is the raw bytes.
    """
    if '\\' not in s:
        return s
    out, i = bytearray(), 0
    esc = {'a': 7, 'b': 8, 'f': 12, 'n': 10, 'r': 13, 't': 9, 'v': 11,
           '\\': 92, '?': 63}
    while i < len(s):
        c = s[i]
        if c == '\\' and i + 1 < len(s):
            n = s[i + 1]
            oct3 = s[i + 1:i + 4]
            if len(oct3) == 3 and all(x in '01234567' for x in oct3):
                out.append(int(oct3, 8) & 0xff)
                i += 4
                continue
            if n in esc:
                out.append(esc[n])
                i += 2
                continue
        out += c.encode('utf-8', 'surrogateescape')
        i += 1
    return out.decode('utf-8', 'surrogateescape')


def canon(aliases, rel):
    """rel without ./ or a trailing /, its top-level alias folded."""
    if rel.startswith('./'):
        rel = rel[2:]
    rel = rel.rstrip('/')
    for a, t in aliases:
        if rel.startswith(a + '/'):
            return t + rel[len(a):]
    return rel


def read_db(root, aliases=None):
    """({package: [canonical paths]}, {canonical path: package}).

    Directories are left out of both: a directory several packages list is
    owned by none of them in particular.
    """
    if aliases is None:
        aliases = canon_load(root)
    db = os.path.join(root, 'var', 'lib', 'kpkg', 'db')
    files, owner = {}, {}
    try:
        names = sorted(os.listdir(db))
    except OSError:
        return files, owner
    for name in names:
        p = os.path.join(db, name)
        if name.startswith('.') or not os.path.isfile(p):
            continue
        with open(p, encoding='utf-8', errors='surrogateescape') as f:
            lines = f.read().split('\n')[1:]
        paths = []
        for ln in lines:
            if not ln or ln.endswith('/'):
                continue
            k = canon(aliases, tar_unquote(ln))
            if not k or k == '.':
                continue
            paths.append(k)
            owner.setdefault(k, name)
        files[name] = paths
    return files, owner


def walk(root, top='', prune=()):
    """(rel, lstat) for every entry under root/top, never following a link.

    `prune` holds relative paths whose whole subtree is skipped.
    """
    stack = [top]
    prune = set(prune)
    while stack:
        rel = stack.pop()
        d = os.path.join(root, rel) if rel else root
        try:
            it = os.scandir(d)
        except OSError:
            continue
        with it:
            for e in it:
                r = rel + '/' + e.name if rel else e.name
                if r in prune:
                    continue
                try:
                    st = e.stat(follow_symlinks=False)
                except OSError:
                    continue
                yield r, st
                if stat.S_ISDIR(st.st_mode):
                    stack.append(r)


def is_elf(path):
    try:
        with open(path, 'rb') as f:
            return f.read(4) == ELF_MAGIC
    except OSError:
        return False


class Elf:
    """The parts of an ELF file the gates read: sections, interpreter, dynamic."""

    def __init__(self, path):
        self.path = path
        self.sections = []      # (name, type, size)
        self.interp = None
        self.needed = []
        self.rpath = []
        self.runpath = []
        self.soname = None
        self.ok = False
        try:
            with open(path, 'rb') as f:
                self._parse(f)
            self.ok = True
        except (OSError, struct.error, ValueError, IndexError):
            pass

    def _parse(self, f):
        h = f.read(64)
        if len(h) < 52 or h[:4] != ELF_MAGIC:
            raise ValueError('not ELF')
        c64 = h[4] == 2
        e = '<' if h[5] == 1 else '>'
        if c64:
            phoff, shoff = struct.unpack_from(e + 'QQ', h, 32)
            phentsize, phnum, shentsize, shnum, shstrndx = struct.unpack_from(
                e + 'HHHHH', h, 54)
        else:
            phoff, shoff = struct.unpack_from(e + 'II', h, 28)
            phentsize, phnum, shentsize, shnum, shstrndx = struct.unpack_from(
                e + 'HHHHH', h, 42)
        f.seek(0, 2)
        fsize = f.tell()

        if shoff and shnum and shoff + shnum * shentsize <= fsize:
            f.seek(shoff)
            raw = f.read(shnum * shentsize)
            sh = []
            for i in range(shnum):
                if c64:
                    nm, ty, _, _, off, size = struct.unpack_from(
                        e + 'IIQQQQ', raw, i * shentsize)
                else:
                    nm, ty, _, _, off, size = struct.unpack_from(
                        e + 'IIIIII', raw, i * shentsize)
                sh.append((nm, ty, off, size))
            if shstrndx < len(sh):
                _, _, soff, ssize = sh[shstrndx]
                f.seek(soff)
                strs = f.read(min(ssize, 1 << 20))
                for nm, ty, _, size in sh:
                    end = strs.find(b'\0', nm)
                    name = strs[nm:end if end >= 0 else None].decode('ascii', 'replace')
                    self.sections.append((name, ty, size))

        if not phoff or not phnum or phoff + phnum * phentsize > fsize:
            return
        f.seek(phoff)
        ph = f.read(phnum * phentsize)
        loads, dyn = [], None
        for i in range(phnum):
            if c64:
                t, _, off, vaddr, _, filesz = struct.unpack_from(
                    e + 'IIQQQQ', ph, i * phentsize)
            else:
                t, off, vaddr, _, filesz = struct.unpack_from(
                    e + 'IIIII', ph, i * phentsize)
            if t == PT_LOAD:
                loads.append((vaddr, off, filesz))
            elif t == PT_DYNAMIC:
                dyn = (off, filesz)
            elif t == PT_INTERP:
                f.seek(off)
                self.interp = f.read(min(filesz, 4096)).split(b'\0', 1)[0].decode(
                    'utf-8', 'replace')
        if not dyn:
            return
        f.seek(dyn[0])
        raw = f.read(dyn[1])
        ent = 16 if c64 else 8
        fmt = e + ('qQ' if c64 else 'iI')
        refs, strtab = [], None
        for i in range(0, len(raw) - ent + 1, ent):
            tag, val = struct.unpack_from(fmt, raw, i)
            if tag == 0:
                break
            if tag in (DT_NEEDED, DT_RPATH, DT_RUNPATH, DT_SONAME):
                refs.append((tag, val))
            elif tag == DT_STRTAB:
                strtab = val
        if strtab is None:
            return
        base = None
        for vaddr, off, filesz in loads:
            if vaddr <= strtab < vaddr + filesz:
                base = strtab - vaddr + off
        if base is None:
            return
        for tag, val in refs:
            f.seek(base + val)
            s = f.read(4096).split(b'\0', 1)[0].decode('utf-8', 'replace')
            if tag == DT_NEEDED:
                self.needed.append(s)
            elif tag == DT_RPATH:
                self.rpath += [p for p in s.split(':') if p]
            elif tag == DT_RUNPATH:
                self.runpath += [p for p in s.split(':') if p]
            else:
                self.soname = s


def resolve(root, rel, target):
    """A link's target as a root-relative path, `..` and aliases folded.

    Absolute targets are taken against root, never the host. Returns None
    when the path climbs out of the root.
    """
    if target.startswith('/'):
        parts = []
        src = target
    else:
        parts = rel.split('/')[:-1]
        src = target
    for p in src.split('/'):
        if p in ('', '.'):
            continue
        if p == '..':
            if not parts:
                return None
            parts.pop()
        else:
            parts.append(p)
    return '/'.join(parts)


def lookup(root, rel, depth=0):
    """Follow rel inside root, every link resolved against root.

    Returns the root-relative path of what it names, or None when a component
    is missing or a link climbs out of the root or loops.
    """
    if depth > 40:
        return None
    parts = [p for p in rel.split('/') if p and p != '.']
    done = []
    for i, p in enumerate(parts):
        if p == '..':
            if not done:
                return None
            done.pop()
            continue
        cur = '/'.join(done + [p])
        full = os.path.join(root, cur)
        try:
            st = os.lstat(full)
        except OSError:
            return None
        if stat.S_ISLNK(st.st_mode):
            try:
                t = os.readlink(full)
            except OSError:
                return None
            nxt = resolve(root, cur, t)
            if nxt is None:
                return None
            rest = '/'.join(parts[i + 1:])
            return lookup(root, nxt + ('/' + rest if rest else ''), depth + 1)
        done.append(p)
    return '/'.join(done)


def exists(root, rel):
    return lookup(root, rel) is not None


def is_exec_file(root, rel):
    """rel resolves inside root to a regular file with an execute bit."""
    r = lookup(root, rel)
    if r is None:
        return False
    try:
        st = os.stat(os.path.join(root, r)) if r else None
    except OSError:
        return False
    return bool(st) and stat.S_ISREG(st.st_mode) and bool(st.st_mode & 0o111)


def which(root, name, path=('usr/bin', 'usr/sbin', 'usr/local/bin', 'usr/local/sbin',
                             'bin', 'sbin')):
    """The root-relative path a bare command name resolves to, or None."""
    for d in path:
        if is_exec_file(root, d + '/' + name):
            return d + '/' + name
    return None
