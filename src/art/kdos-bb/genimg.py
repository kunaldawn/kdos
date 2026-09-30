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
# Regenerate src/kdostux.c — the mascot and the wordmark as one bb image.
#
# HOST ONLY. Needs Pillow and a C compiler, has network access to neither,
# and nothing on the target or in the build ever runs it. Its OUTPUT is
# committed, the same arrangement genlogo.py and the three art vendor.py
# scripts already use.
#
# The demo stores every photograph as an LZO1X-compressed 8-bit greyscale
# blob behind a `struct image` (see fk1.c and image.h), and image.c's
# decompressimg() calls lzo1x_decompress on it unconditionally. So the
# mascot has to arrive in exactly that format or it cannot be drawn by
# dispimg() — which is the whole point of choosing this route, because
# dispimg is the demo's own scaling, dithering and strobe path and a
# picture that goes through it looks like it was always in the demo.
#
# The compressor is minilzo.c out of src/, compiled here on demand. Using
# the same implementation on both ends is not a convenience: LZO1X has
# several encoders and only the matching decoder is guaranteed.
#
#   ./genimg.py ../../../kdos.png

import os, subprocess, sys, tempfile
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
OUT  = os.path.join(HERE, "src", "kdostux.c")
NAME = "kdostux"

# Tall, because the source is: mascot above, wordmark below. dispimg()
# LETTERBOXES rather than crops -- a target wider than the image gives a
# negative x1 and scale() centres it against black -- so the whole mark
# survives on any terminal shape, which a crop would not.
W, H = 220, 302

# The mascot's outline is a dark rim against a dark background and the AA
# renderer only has the luminance to work with, so the black end is lifted
# off the floor and the range stretched. Measured by eye against
# `aview`-style output, not guessed at: without the lift the rim closes up
# and the penguin loses its silhouette entirely at 80x24.
FLOOR, GAMMA = 18, 0.85

CSRC = r'''
#include <stdio.h>
#include <stdlib.h>
#include "minilzo.h"
static unsigned char wrk[LZO1X_MEM_COMPRESS];
int main(int argc, char **argv)
{
    FILE *f = fopen(argv[1], "rb");
    long n = atol(argv[2]);
    unsigned char *in = malloc(n), *out = malloc(n + n / 16 + 64 + 3);
    lzo_uint outlen = 0;
    long i;
    if (!f || fread(in, 1, n, f) != (size_t) n) return 1;
    if (lzo_init() != LZO_E_OK) return 2;
    if (lzo1x_1_compress(in, n, out, &outlen, wrk) != LZO_E_OK) return 3;
    printf("%lu\n", (unsigned long) outlen);
    for (i = 0; i < (long) outlen; i++)
        printf("%d%s", out[i], i + 1 < (long) outlen ? "," : "");
    return 0;
}
'''

CHECK = r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "minilzo.h"
int main(int argc, char **argv)
{
    FILE *f = fopen(argv[1], "rb");
    long clen = atol(argv[2]), rlen = atol(argv[3]);
    unsigned char *c = malloc(clen), *d = malloc(rlen + 64), *o = malloc(rlen);
    lzo_uint out = rlen;
    FILE *g;
    if (!f || fread(c, 1, clen, f) != (size_t) clen) return 1;
    if (lzo_init() != LZO_E_OK) return 2;
    if (lzo1x_decompress(c, clen, d, &out, NULL) != LZO_E_OK) return 3;
    if ((long) out != rlen) { fprintf(stderr, "length %lu != %ld\n",
                                      (unsigned long) out, rlen); return 4; }
    g = fopen(argv[4], "rb");
    if (!g || fread(o, 1, rlen, g) != (size_t) rlen) return 5;
    if (memcmp(o, d, rlen)) { fprintf(stderr, "payload differs\n"); return 6; }
    return 0;
}
'''

def main():
    png = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "../../../kdos.png")
    im = Image.open(png).convert("RGBA")
    # Composite onto black first: the source is RGBA and a straight convert
    # takes the colour of fully transparent pixels, which here is white.
    im = Image.alpha_composite(Image.new("RGBA", im.size, (0, 0, 0, 255)), im)
    box = im.split()[3].getbbox() or im.getbbox()
    g = im.convert("L").crop(box).resize((W, H), Image.LANCZOS)
    px = bytearray(g.tobytes())
    for i, v in enumerate(px):
        px[i] = min(255, int(FLOOR + (255 - FLOOR) * ((v / 255.0) ** GAMMA)) if v else 0)

    raw = os.path.join(tempfile.mkdtemp(), "raw")
    if True:
        td = os.path.dirname(raw)
        open(raw, "wb").write(bytes(px))
        csrc = os.path.join(td, "pack.c")
        open(csrc, "w").write(CSRC)
        exe = os.path.join(td, "pack")
        subprocess.run(["cc", "-O2", "-I", os.path.join(HERE, "src"), "-o", exe,
                        csrc, os.path.join(HERE, "src", "minilzo.c")], check=True)
        r = subprocess.run([exe, raw, str(len(px))], check=True, capture_output=True, text=True)

    size, data = r.stdout.split("\n", 1)

    # Round-trip through the same decoder image.c will use. An asset that
    # decompresses to the wrong length draws as noise or walks off the end
    # of the buffer, and neither says which file was wrong -- so it is
    # checked here, where the answer is one line, rather than in a demo.
    with tempfile.TemporaryDirectory() as td:
        chk = os.path.join(td, "chk.c")
        open(chk, "w").write(CHECK)
        exe = os.path.join(td, "chk")
        subprocess.run(["cc", "-O2", "-I", os.path.join(HERE, "src"), "-o", exe,
                        chk, os.path.join(HERE, "src", "minilzo.c")], check=True)
        open(os.path.join(td, "blob"), "wb").write(
            bytes(int(v) for v in data.strip().split(",")))
        subprocess.run([exe, os.path.join(td, "blob"), size, str(len(px)), raw],
                       check=True)

    vals = data.strip().split(",")
    body = []
    for i in range(0, len(vals), 16):
        body.append("    " + ", ".join(vals[i:i + 16]) + ("," if i + 16 < len(vals) else "")) 

    with open(OUT, "w") as f:
        f.write("/* Automatically generated image %s -- see genimg.py, do not hand-edit.\n"
                " * kdos.png, greyscaled and LZO1X-compressed into the format image.h\n"
                " * describes and dispimg() draws. */\n"
                "#include \"image.h\"\n"
                "static unsigned char %sdata[] =\n{\n%s\n};\n"
                "struct image %s =\n{%sdata, %s, %d, %d};\n"
                % (NAME, NAME, "\n".join(body), NAME, NAME, size, W, H))
    print("%s: %dx%d, %s bytes compressed from %d (%.1fx)"
          % (OUT, W, H, size, len(px), len(px) / int(size)))

main()
