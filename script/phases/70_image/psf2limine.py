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
# A console PSF font, rewritten as the raw bitmap Limine's `term_font` wants.
#
# THE TWO FORMATS DISAGREE ABOUT ORDER, AND THAT IS THE WHOLE JOB. A PSF is
# indexed by the font's own glyph number and carries a unicode table saying
# which codepoints each glyph draws; Limine has no table and indexes by CP437,
# so glyph 218 is whatever CP437 calls 218 — the top-left double corner. Copied
# across unconverted the bitmap is the right size and every frame, rule and
# arrow in the boot menu draws a different letter. The size being right is why
# this cannot be caught by looking at the file.
#
# THE OUTPUT IS HEADERLESS: 256 glyphs of `charsize` bytes and nothing else,
# which is what Limine reads once `term_font_size` tells it the dimensions.
# A header left on shifts every glyph and the menu draws garbage from the
# first character.

import sys


def parse(data):
    """(width, height, charsize, [glyph bitmaps], {codepoint: glyph index})"""
    if data[:2] == b"\x36\x04":
        # PSF1: mode bit 0 is the 512-glyph flag, bit 1 a unicode table.
        mode, charsize = data[2], data[3]
        n = 512 if mode & 1 else 256
        has_uni = bool(mode & 2)
        width, height = 8, charsize
        off = 4
    elif data[:4] == b"\x72\xb5\x4a\x86":
        import struct
        (_, _, hs, flags, n, charsize, height,
         width) = struct.unpack("<8I", data[:32])
        has_uni = bool(flags & 1)
        off = hs
    else:
        raise SystemExit("not a PSF font")

    glyphs = [data[off + i * charsize: off + (i + 1) * charsize]
              for i in range(n)]
    uni = {}
    if has_uni:
        tbl = data[off + n * charsize:]
        if data[:2] == b"\x36\x04":
            # uint16 LE; 0xffff ends a glyph, 0xfffe separates sequences.
            idx, i = 0, 0
            while i + 1 < len(tbl) and idx < n:
                cp = tbl[i] | (tbl[i + 1] << 8)
                i += 2
                if cp == 0xFFFF:
                    idx += 1
                elif cp != 0xFFFE:
                    uni.setdefault(cp, idx)
        else:
            # UTF-8; 0xff ends a glyph, 0xfe separates sequences.
            idx, cur = 0, bytearray()
            for b in tbl:
                if b == 0xFF:
                    for ch in cur.decode("utf-8", "ignore"):
                        uni.setdefault(ord(ch), idx)
                    cur.clear()
                    idx += 1
                    if idx >= n:
                        break
                elif b == 0xFE:
                    for ch in cur.decode("utf-8", "ignore"):
                        uni.setdefault(ord(ch), idx)
                    cur.clear()
                else:
                    cur.append(b)
    return width, height, charsize, glyphs, uni


# CP437's FIRST 32 SLOTS AND 0x7F ARE GLYPHS, NOT CONTROL CODES, and Python's
# `cp437` codec decodes them the other way — to U+0001..U+001F, which no font
# has. Taken from the codec, a third of the font comes out blank and the two
# arrow glyphs Limine draws its menu selection with are among them. Only this
# table says what those slots actually draw.
CP437_LOW = ("\u0000\u263a\u263b\u2665\u2666\u2663\u2660\u2022"
             "\u25d8\u25cb\u25d9\u2642\u2640\u266a\u266b\u263c"
             "\u25ba\u25c4\u2195\u203c\u00b6\u00a7\u25ac\u21a8"
             "\u2191\u2193\u2192\u2190\u221f\u2194\u25b2\u25bc")


def cp437(i):
    if i < 32:
        return ord(CP437_LOW[i])
    if i == 0x7F:
        return 0x2302          # ⌂, not DEL
    return ord(bytes([i]).decode("cp437"))


def main():
    if len(sys.argv) != 3:
        raise SystemExit("usage: psf2limine.py <font.psf> <out.bin>")
    data = open(sys.argv[1], "rb").read()
    width, height, charsize, glyphs, uni = parse(data)

    # LIMINE ONLY READS 8-PIXEL-WIDE FONTS, and a wider one is not an error
    # there: `term_font_size` refuses any width but 8 and the built-in font is
    # used in place of `term_font`. The menu then draws perfectly in the wrong
    # face, which is indistinguishable from the theming simply not having been
    # applied. Terminus at 16 dots wide converts and installs without
    # complaint, so this is the only place the mistake is visible.
    if width != 8:
        raise SystemExit("psf2limine: %s is %d dots wide — Limine reads only "
                         "8-wide fonts and silently ignores the rest"
                         % (sys.argv[1], width))

    # A FONT WITH NO UNICODE TABLE IS ALREADY IN ITS OWN ORDER and there is
    # nothing to map it by, so it is taken as-is. Every console font KDOS
    # ships has one; this is what keeps a hand-made font from silently
    # producing 256 copies of glyph zero.
    blank = bytes(charsize)
    out = bytearray()
    missing = 0
    for i in range(256):
        if uni:
            cp = cp437(i)
            g = uni.get(cp)
            if g is None:
                missing += 1
                out += blank
                continue
            out += glyphs[g]
        else:
            out += glyphs[i] if i < len(glyphs) else blank

    open(sys.argv[2], "wb").write(bytes(out))
    sys.stderr.write("psf2limine: %dx%d, %d bytes/glyph, %d of 256 missing\n"
                     % (width, height, charsize, missing))
    # A CP437-ENCODED FONT ANSWERS FOR ALL 256 AND ANYTHING LESS IS THE WRONG
    # FILE. Terminus ships both encodings under names one letter apart:
    # `ter-i32n` is the CP437 one and scores zero here, `ter-kdos32n` is the
    # console's own and leaves 46 slots blank — every double-line box glyph
    # among them, so the menu chrome comes out full of holes. Both convert
    # without error and both are the right size, so nothing downstream can
    # tell them apart. This is the only place that can.
    if missing:
        raise SystemExit("psf2limine: %d of 256 glyphs missing — %s is not "
                         "CP437-encoded" % (missing, sys.argv[1]))
    print("%dx%d" % (width, height))


main()
