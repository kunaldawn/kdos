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
# Re-vendor the cursor artwork in art/ from an upstream Bibata release.
#
# This is a MAINTENANCE tool, not part of the build: it runs on the host, with
# network, when the artwork should be refreshed. The build only ever reads the
# pruned art/ committed next to it.
#
#   vendor.py <Bibata-Modern-Ice.tar.xz>
#
# What it throws away, and why:
#   * Sizes. Upstream ships 14 (16..96). KDOS keeps five: 24 is the default
#     (XCURSOR_SIZE), 48 covers 2x HiDPI, 96 covers 4x, 32 and 64 fill in
#     between for fractional scales. That alone is a ~3.5x cut.
#   * Animation frames. `wait` and `progress` ship 54 frames each and are 9 MB
#     apiece — over two thirds of the whole theme. Every third frame is kept and
#     the per-frame delay is tripled, which is visually identical at this speed.
#   * Shapes KDOS has no use for: X11 relics (pirate/X_cursor, dotbox, draped
#     box, target, the angle and tee corner shapes, sb_*_arrow scrollbar
#     arrows, right_ptr, draft, cross variants, colour picker). What is left is
#     the freedesktop/CSS set plus the resize and dnd names toolkits actually
#     request.
#
# The result is still upstream's monochrome artwork; recolouring happens at
# build time in gencursors.py so the palette stays a KDOS decision.

import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ART = os.path.join(HERE, 'art')

IMAGE_TYPE = 0xFFFD0002
KEEP_SIZES = (24, 32, 48, 64, 96)
FRAME_STRIDE = 3

# Canonical shape -> alias names the theme should also answer to. Canonical
# names are the CSS ones so that a stale symlink from an older theme is
# overwritten by an identical one instead of forming a link loop.
SHAPES = {
    'default': ['arrow', 'left_ptr', 'top_left_arrow'],
    'pointer': ['hand2', 'pointing_hand', '9d800788f1b08800ae810202380a0822',
                'e29285e634086352946a0e7090d73106'],
    'grab': ['hand1', 'openhand'],
    'dnd-move': ['closedhand', 'dnd-none', 'grabbing',
                 'fcf21c00b30f7e3f83fe0dfd12e71cff'],
    'text': ['ibeam', 'xterm'],
    'vertical-text': [],
    'crosshair': [],
    'cell': ['plus'],
    'context-menu': [],
    'help': ['left_ptr_help', 'question_arrow', 'whats_this',
             '5c6cd98b3f3ebcb1f9c7f1c204630408',
             'd9ce0ab605698f320427677b458ad60b'],
    'copy': ['dnd-copy', '1081e37283d90000800003c07f3ef6bf',
             '6407b0e94181790501fd1e167b474872',
             'b66166c04f8c3109214a4fbd64a50fc8'],
    'alias': ['dnd-link', 'link'],
    'no-drop': ['dnd_no_drop'],
    'not-allowed': ['crossed_circle', 'forbidden',
                    '03b6e0fcb3499374a867c041f52298f0'],
    'move': ['all-scroll', 'fleur', 'size_all',
             '4498f0e0c1937ffe01fd06f973665830',
             '9081237383d90e509aa00f00170e968f'],
    'wait': ['watch'],
    'progress': ['left_ptr_watch', '00000000000000020006000e7e9ffc3f',
                 '08e8e1c95fe2fc01f976f1e063a24ccd',
                 '3ecb610c1bf2410f44200f48c40d3599'],
    'zoom-in': [],
    'zoom-out': [],
    'ew-resize': ['col-resize', 'h_double_arrow', 'sb_h_double_arrow',
                  'size_hor', 'split_h', '028006030e0e7ebffc7f7070c0600140',
                  '14fef782d02440884392942c1120523'],
    'ns-resize': ['row-resize', 'v_double_arrow', 'sb_v_double_arrow',
                  'size_ver', 'split_v', 'double_arrow',
                  '00008160000006810000408080010102',
                  '2870a09082c103050810ffdffffe0204'],
    'nesw-resize': ['fd_double_arrow', 'size_bdiag',
                    'fcf1c3c7cd4491d801f1e1c78f100000'],
    'nwse-resize': ['bd_double_arrow', 'size_fdiag',
                    'c7088f0f3e6c8088236ef8e1e3e70000'],
    'n-resize': ['top_side'],
    's-resize': ['bottom_side'],
    'e-resize': ['right_side'],
    'w-resize': ['left_side'],
    'ne-resize': ['top_right_corner'],
    'nw-resize': ['top_left_corner'],
    'se-resize': ['bottom_right_corner'],
    'sw-resize': ['bottom_left_corner'],
    'wayland-cursor': [],
}


def read_chunks(path):
    d = open(path, 'rb').read()
    if d[:4] != b'Xcur':
        raise ValueError('%s is not an Xcursor file' % path)
    _hsz, _ver, ntoc = struct.unpack('<3I', d[4:16])
    out = []
    for i in range(ntoc):
        ctype, subtype, pos = struct.unpack('<3I', d[16 + i * 12:28 + i * 12])
        (clen,) = struct.unpack('<I', d[pos:pos + 4])
        if ctype == IMAGE_TYPE:
            w, h = struct.unpack('<2I', d[pos + 16:pos + 24])
            end = pos + 36 + 4 * w * h
        else:
            end = pos + clen
        out.append((ctype, subtype, d[pos:end]))
    return out


def write_chunks(path, chunks):
    pos = 16 + len(chunks) * 12
    toc = b''
    body = b''
    for ctype, subtype, payload in chunks:
        toc += struct.pack('<3I', ctype, subtype, pos)
        body += payload
        pos += len(payload)
    with open(path, 'wb') as f:
        f.write(b'Xcur' + struct.pack('<3I', 16, 0x10000, len(chunks)))
        f.write(toc)
        f.write(body)


def prune(src, dst):
    chunks = read_chunks(src)
    per_size = {}
    for ctype, subtype, payload in chunks:
        if ctype != IMAGE_TYPE:
            continue
        per_size.setdefault(subtype, []).append(payload)

    out = []
    for size in KEEP_SIZES:
        frames = per_size.get(size)
        if not frames:
            continue
        if len(frames) > 1:
            kept = frames[::FRAME_STRIDE]
            fixed = []
            for payload in kept:
                head = bytearray(payload[:36])
                (delay,) = struct.unpack_from('<I', head, 32)
                struct.pack_into('<I', head, 32, delay * FRAME_STRIDE)
                fixed.append(bytes(head) + payload[36:])
            frames = fixed
        for payload in frames:
            out.append((IMAGE_TYPE, size, payload))
    write_chunks(dst, out)
    return len(out)


def main():
    tarball = sys.argv[1]
    tmp = tempfile.mkdtemp(prefix='kdos-cursors-')
    try:
        subprocess.run(['tar', 'xf', tarball, '-C', tmp, '--strip-components=1'],
                       check=True)
        cursors = os.path.join(tmp, 'cursors')

        if os.path.isdir(ART):
            shutil.rmtree(ART)
        os.makedirs(ART)

        total = 0
        for shape in sorted(SHAPES):
            src = os.path.join(cursors, shape)
            if not os.path.exists(src):
                raise SystemExit('upstream has no shape %r' % shape)
            src = os.path.realpath(src)
            dst = os.path.join(ART, shape)
            n = prune(src, dst)
            total += os.path.getsize(dst)
            print('  %-16s %2d images  %6d bytes' % (shape, n, os.path.getsize(dst)))

        ver = re.search(r'(\d+\.\d+\.\d+)', os.path.basename(tarball))
        with open(os.path.join(ART, 'UPSTREAM'), 'w') as f:
            f.write('Bibata-Modern-Ice %s\n' % (ver.group(1) if ver else 'unknown'))
        print('kdos-cursors: vendored %d shapes, %d KiB total'
              % (len(SHAPES), total // 1024))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


main()
