#!/bin/bash
# ██╗  ██╗██████╗  ██████╗ ███████╗
# ██║ ██╔╝██╔══██╗██╔═══██╗██╔════╝
# █████╔╝ ██║  ██║██║   ██║███████╗
# ██╔═██╗ ██║  ██║██║   ██║╚════██║
# ██║  ██╗██████╔╝╚██████╔╝███████║
# ╚═╝  ╚═╝╚═════╝  ╚═════╝ ╚══════╝
# ---------------------------------
#   KD's Homebrew Linux Distro
# ---------------------------------


# NotoColorEmoji.ttf is compiled here from the 128-pixel PNG artwork and the
# region-flag images by upstream's Makefile: waveflag (C, cairo) waves each
# flag, ImageMagick centres every image on the 136x128 canvas, pngquant
# reduces it to a palette and zopflipng recompresses it, and the builder
# scripts assemble the CBDT/CBLC tables with fontTools. The built faces the
# tarball carries under fonts/ are removed first, so the one installed is the
# one built here.
#
# CBDT AND NOT COLRv1. Two builds exist: the bitmap one every FreeType can
# draw, and the vector one that needs a rasteriser this tree does not have.
# libkcell renders through FreeType, so the bitmap face is the one that
# appears rather than the one that is technically newer. The COLRv1 build is
# nanoemoji's, and nothing here runs it.
#
# nototools is a second source used in place, not a port: the build needs
# its font_data and unicode_data modules and its add_vs_cmap.py script, and
# nothing on the target does. The Makefile refuses to run outside a Python
# virtual environment, which VIRTUAL_ENV satisfies; the interpreter it would
# find there is the system one. add_glyphs.py imports the PNG reader that
# sits beside the builder in third_party/color_emoji.
#
# BYPASS_SEQUENCE_CHECK: the check measures the artwork against the emoji
# list in nototools' Unicode data, and nototools $_nototools carries a newer
# Emoji version than this release draws, so it fails on every emoji added
# after it. The check reads nothing into the font.
rm -f fonts/*.ttf
nototools="$SRC_ROOT/notofonttools-$_nototools"
export PYTHONPATH="$nototools:$SRC/third_party/color_emoji"
make -j"$(nproc)" NotoColorEmoji.ttf \
	VIRTUAL_ENV=/usr BYPASS_SEQUENCE_CHECK=True \
	VS_ADDER="python3 $nototools/nototools/add_vs_cmap.py"
install -Dm644 NotoColorEmoji.ttf \
	"$PKG/usr/share/fonts/noto-emoji/NotoColorEmoji.ttf"
install -Dm644 fonts/LICENSE "$PKG/usr/share/licenses/$name/LICENSE"

# AND FONTCONFIG MUST BE TOLD IT IS THE EMOJI FACE. `70-no-bitmaps-except-emoji`
# already carves bitmap fonts an exception BY FAMILY NAME; without a rule that
# names this family for the generic `emoji` alias, a program asking for emoji
# gets whatever the sans face has, which is nothing, and every codepoint is a
# blank box.
install -d "$PKG/usr/share/fontconfig/conf.avail"
cat > "$PKG/usr/share/fontconfig/conf.avail/45-noto-color-emoji.conf" <<'XML'
<?xml version="1.0"?>
<!DOCTYPE fontconfig SYSTEM "urn:fontconfig:fonts.dtd">
<fontconfig>
  <alias binding="same">
    <family>emoji</family>
    <accept><family>Noto Color Emoji</family></accept>
  </alias>
  <match target="pattern">
    <test qual="any" name="family"><string>emoji</string></test>
    <edit name="family" mode="assign" binding="same"><string>Noto Color Emoji</string></edit>
  </match>
</fontconfig>
XML
install -d "$PKG/etc/fonts/conf.d"
ln -sf /usr/share/fontconfig/conf.avail/45-noto-color-emoji.conf \
	"$PKG/etc/fonts/conf.d/45-noto-color-emoji.conf"
