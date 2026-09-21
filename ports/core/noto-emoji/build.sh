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


# THE BUILT FONT IS IN THE TARBALL and nothing here regenerates it. Building
# Noto Color Emoji from its sources needs nanoemoji, a PNG pipeline and a
# network, and the result is the file upstream already ships.
#
# CBDT AND NOT COLRv1. Two builds exist: the bitmap one every FreeType can
# draw, and the vector one that needs a rasteriser this tree does not have.
# libkcell renders through FreeType, so the bitmap face is the one that
# appears rather than the one that is technically newer.
install -Dm644 fonts/NotoColorEmoji.ttf \
	"$PKG/usr/share/fonts/noto-emoji/NotoColorEmoji.ttf"

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
