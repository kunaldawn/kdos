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

rm -f fonts/*.ttf
nototools="$SRC_ROOT/notofonttools-$_nototools"
export PYTHONPATH="$nototools:$SRC/third_party/color_emoji"
make -j"$KDOS_JOBS" NotoColorEmoji.ttf \
	VIRTUAL_ENV=/usr BYPASS_SEQUENCE_CHECK=True \
	VS_ADDER="python3 $nototools/nototools/add_vs_cmap.py"
install -Dm644 NotoColorEmoji.ttf \
	"$PKG/usr/share/fonts/noto-emoji/NotoColorEmoji.ttf"
install -Dm644 fonts/LICENSE "$PKG/usr/share/licenses/$name/LICENSE"

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
