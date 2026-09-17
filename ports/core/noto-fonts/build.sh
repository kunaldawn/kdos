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

# kpkg copies a .zip into $SRC rather than unpacking it, so the recipe does.
# Each into its own directory: both carry a top-level OFL.txt and Name.
unzip -q -o $name-$version.zip -d sans
unzip -q -o $name-mono-$_mono.zip -d mono

# The upright and italic faces at the five weights the desktop asks for, and
# the hinted build to match the slight-hinting rule in /etc/fonts/conf.d. The
# condensed widths, the variable builds and the OTF set are the same glyphs
# again; nothing here names them.
install -dm755 $PKG/usr/share/fonts/noto
for f in Regular Italic Medium MediumItalic SemiBold SemiBoldItalic Bold BoldItalic Light LightItalic; do
	install -m644 sans/NotoSans/hinted/ttf/NotoSans-$f.ttf \
		$PKG/usr/share/fonts/noto/
done
# Noto Sans Mono ships no italic at any weight; fontconfig's 90-synthetic.conf
# shears the upright one, which is what an italic cell in the grid gets.
for f in Regular Medium SemiBold Bold Light; do
	install -m644 mono/NotoSansMono/hinted/ttf/NotoSansMono-$f.ttf \
		$PKG/usr/share/fonts/noto/
done

install -Dm644 sans/OFL.txt $PKG/usr/share/licenses/$name/OFL.txt

# Numbered below 57-dejavu-*.conf so Noto leads the generic families and DejaVu
# is the face fontconfig reaches next. That order is load-bearing: Noto Sans
# Mono carries no diagonal arrows, no dingbats and almost no misc symbols, and
# fcft resolves a codepoint its primary face lacks down the fontconfig list.
install -dm755 $PKG/etc/fonts/conf.d
cat > $PKG/etc/fonts/conf.d/56-noto-preferred.conf <<'XML'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE fontconfig SYSTEM "../fonts.dtd">
<fontconfig>
  <alias>
    <family>Noto Sans</family>
    <default><family>sans-serif</family></default>
  </alias>
  <alias>
    <family>Noto Sans Mono</family>
    <default><family>monospace</family></default>
  </alias>
  <alias>
    <family>sans-serif</family>
    <prefer><family>Noto Sans</family></prefer>
  </alias>
  <alias>
    <family>monospace</family>
    <prefer><family>Noto Sans Mono</family></prefer>
  </alias>
</fontconfig>
XML
