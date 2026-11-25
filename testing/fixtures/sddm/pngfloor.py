#!/usr/bin/env python3
# The contrast of a text colour against the brightest pixel of each PNG named,
# one ratio per line, two decimals, as WCAG 2 measures it. Standard library
# only, so the selftest needs no imaging package: 8-bit RGB and RGBA, no
# interlace, which is what kdos-theme writes. --corner prints each PNG's
# top-left pixel as R,G,B instead, the ground a picture's edge shows.
#
#   pngfloor.py R,G,B FILE.png...
#   pngfloor.py --corner FILE.png...
import struct
import sys
import zlib


def lum(px):
    lin = [v / 255 for v in px]
    lin = [v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4 for v in lin]
    return 0.2126 * lin[0] + 0.7152 * lin[1] + 0.0722 * lin[2]


def rows(path):
    data = open(path, 'rb').read()
    if data[:8] != b'\x89PNG\r\n\x1a\n':
        sys.exit(f'{path}: not a PNG')
    i, idat = 8, b''
    while i < len(data):
        n, kind = struct.unpack('>I4s', data[i:i + 8])
        if kind == b'IHDR':
            w, h, depth, ctype, _, _, lace = struct.unpack('>IIBBBBB', data[i + 8:i + 21])
            if depth != 8 or ctype not in (2, 6) or lace:
                sys.exit(f'{path}: not 8-bit RGB or RGBA without interlace')
        elif kind == b'IDAT':
            idat += data[i + 8:i + 8 + n]
        i += 12 + n
    bpp = 3 if ctype == 2 else 4
    raw, stride = zlib.decompress(idat), w * bpp
    prev, o = bytearray(stride), 0
    for _ in range(h):
        filt, row = raw[o], bytearray(raw[o + 1:o + 1 + stride])
        o += 1 + stride
        for x in range(stride):
            a = row[x - bpp] if x >= bpp else 0
            b = prev[x]
            c = prev[x - bpp] if x >= bpp else 0
            if filt == 1:
                row[x] = (row[x] + a) & 255
            elif filt == 2:
                row[x] = (row[x] + b) & 255
            elif filt == 3:
                row[x] = (row[x] + (a + b) // 2) & 255
            elif filt == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                row[x] = (row[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        yield [row[x:x + 3] for x in range(0, stride, bpp)]
        prev = row


def brightest(path):
    return max(lum(px) for row in rows(path) for px in row)


if sys.argv[1] == '--corner':
    for f in sys.argv[2:]:
        print(','.join(str(v) for v in next(rows(f))[0]))
    sys.exit(0)
text = lum([int(v) for v in sys.argv[1].split(',')])
for f in sys.argv[2:]:
    print('%.2f' % ((text + 0.05) / (brightest(f) + 0.05)))
