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
# Build the KDOS cursor theme from the vendored artwork in art/.
#
# art/ holds monochrome Xcursor shapes (white body, black outline, grey
# anti-aliasing), pruned by vendor.py from a Bibata-Modern-Ice release. This
# script paints them in the KDOS palette and lays out the theme:
#
#   * Every pixel's luminance is mapped onto a phosphor ramp, dark green to
#     accent, so the artwork stops being greyscale and starts being KDOS.
#   * Busy shapes (wait, progress) ramp to AMBER instead — the same "working on
#     it" colour the boot splash uses for a pending stage.
#   * Xcursor ARGB is PREMULTIPLIED: alpha is divided out before the ramp and
#     multiplied back after, or every anti-aliased edge becomes a dark halo.
#   * The real file of each alias group is the CSS name and every X11 name is a
#     symlink to it. That direction matters on upgrades — the theme this
#     replaced was laid out the same way, so a stale `left_ptr -> default` is
#     overwritten by an identical link instead of forming a loop that makes
#     kpkg abort with "Symbolic link loop".
#
#   gencursors.py <outdir>              write the theme
#   gencursors.py <outdir> --preview <png>   contact sheet, to eyeball it
#
# Palette must match the phosphor row in fs/usr/local/bin/kdos.

import os
import struct
import sys

OUTLINE = (0x04, 0x12, 0x0a)     # luminance 0 — the outline
ACCENT = (0x39, 0xff, 0x14)      # luminance 1 — the body
AMBER = (0xff, 0xb0, 0x00)       # luminance 1 — busy shapes only

BUSY_SHAPES = {'wait', 'progress'}

IMAGE_TYPE = 0xFFFD0002

HERE = os.path.dirname(os.path.abspath(__file__))
ART = os.path.join(HERE, 'art')

# Canonical shape -> X11/legacy names that must resolve to it. Keep in sync
# with SHAPES in vendor.py; anything listed here needs art/<shape> to exist.
ALIASES = {
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


def ramp(top):
    return [tuple(int(round(OUTLINE[c] + (top[c] - OUTLINE[c]) * (i / 255.0)))
                  for c in range(3)) for i in range(256)]


LUT_ACCENT = ramp(ACCENT)
LUT_AMBER = ramp(AMBER)


def recolor(data, count, lut):
    out = bytearray(len(data))
    for i in range(count):
        v = struct.unpack_from('<I', data, i * 4)[0]
        a = (v >> 24) & 0xFF
        if not a:
            continue
        r, g, b = (v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF
        if a != 0xFF:                            # un-premultiply
            r = min(255, r * 255 // a)
            g = min(255, g * 255 // a)
            b = min(255, b * 255 // a)
        nr, ng, nb = lut[(r * 299 + g * 587 + b * 114) // 1000]
        if a != 0xFF:                            # re-premultiply
            nr, ng, nb = nr * a // 255, ng * a // 255, nb * a // 255
        struct.pack_into('<I', out, i * 4, (a << 24) | (nr << 16) | (ng << 8) | nb)
    return bytes(out)


def convert(src, dst, lut):
    d = open(src, 'rb').read()
    if d[:4] != b'Xcur':
        raise ValueError('%s is not an Xcursor file' % src)
    _hsz, _ver, ntoc = struct.unpack('<3I', d[4:16])
    chunks = []
    for i in range(ntoc):
        ctype, subtype, pos = struct.unpack('<3I', d[16 + i * 12:28 + i * 12])
        if ctype == IMAGE_TYPE:
            w, h = struct.unpack('<2I', d[pos + 16:pos + 24])
            payload = d[pos:pos + 36] + recolor(d[pos + 36:pos + 36 + 4 * w * h],
                                                w * h, lut)
        else:
            (clen,) = struct.unpack('<I', d[pos:pos + 4])
            payload = d[pos:pos + clen]
        chunks.append((ctype, subtype, payload))

    pos = 16 + len(chunks) * 12
    toc = body = b''
    for ctype, subtype, payload in chunks:
        toc += struct.pack('<3I', ctype, subtype, pos)
        body += payload
        pos += len(payload)
    with open(dst, 'wb') as f:
        f.write(b'Xcur' + struct.pack('<3I', 16, 0x10000, len(chunks)))
        f.write(toc)
        f.write(body)


def preview(theme_dir, path):
    from PIL import Image
    names = ['default', 'pointer', 'text', 'wait', 'progress', 'not-allowed',
             'help', 'copy', 'move', 'ns-resize', 'nwse-resize', 'zoom-in']
    tiles = []
    for name in names:
        d = open(os.path.join(theme_dir, 'cursors', name), 'rb').read()
        _h, _v, ntoc = struct.unpack('<3I', d[4:16])
        for i in range(ntoc):
            ctype, subtype, pos = struct.unpack('<3I', d[16 + i * 12:28 + i * 12])
            if ctype == IMAGE_TYPE and subtype == 48:
                w, h = struct.unpack('<2I', d[pos + 16:pos + 24])
                px = struct.unpack('<%dI' % (w * h), d[pos + 36:pos + 36 + 4 * w * h])
                im = Image.new('RGBA', (w, h))
                for y in range(h):
                    for x in range(w):
                        v = px[y * w + x]
                        im.putpixel((x, y), ((v >> 16) & 255, (v >> 8) & 255,
                                             v & 255, (v >> 24) & 255))
                tiles.append(im)
                break
    cols = 6
    rows = (len(tiles) + cols - 1) // cols
    cell = max(max(i.size) for i in tiles) + 12
    sheet = Image.new('RGB', (cols * cell, rows * cell), (0, 10, 3))
    for k, im in enumerate(tiles):
        sheet.paste(im, ((k % cols) * cell + 6, (k // cols) * cell + 6), im)
    sheet.resize((sheet.width * 2, sheet.height * 2), Image.NEAREST).save(path)


def main():
    outdir = sys.argv[1]
    curdir = os.path.join(outdir, 'cursors')
    os.makedirs(curdir, exist_ok=True)

    shapes = links = 0
    for shape, aliases in sorted(ALIASES.items()):
        src = os.path.join(ART, shape)
        if not os.path.exists(src):
            raise SystemExit('art/%s is missing — re-run vendor.py' % shape)
        lut = LUT_AMBER if shape in BUSY_SHAPES else LUT_ACCENT
        convert(src, os.path.join(curdir, shape), lut)
        shapes += 1
        for alias in aliases:
            link = os.path.join(curdir, alias)
            if os.path.lexists(link):
                os.unlink(link)
            os.symlink(shape, link)
            links += 1

    with open(os.path.join(outdir, 'index.theme'), 'w') as f:
        f.write('[Icon Theme]\n'
                'Name=KDOS-cursors\n'
                'Comment=KDOS phosphor cursors\n'
                'Inherits=\n')
    with open(os.path.join(outdir, 'cursor.theme'), 'w') as f:
        f.write('[Icon Theme]\nName=KDOS-cursors\nInherits=KDOS-cursors\n')

    print('kdos-cursors: %d shapes, %d aliases -> %s' % (shapes, links, curdir))

    if '--preview' in sys.argv:
        preview(outdir, sys.argv[sys.argv.index('--preview') + 1])


main()
