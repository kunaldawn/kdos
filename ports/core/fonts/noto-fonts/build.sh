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

unzip -q -o $name-$version.zip -d sans
unzip -q -o $name-mono-$_mono.zip -d mono

install -dm755 $PKG/usr/share/fonts/noto
for f in Regular Italic Medium MediumItalic SemiBold SemiBoldItalic Bold BoldItalic Light LightItalic; do
	install -m644 sans/NotoSans/hinted/ttf/NotoSans-$f.ttf \
		$PKG/usr/share/fonts/noto/
done
for f in Regular Medium SemiBold Bold Light; do
	install -m644 mono/NotoSansMono/hinted/ttf/NotoSansMono-$f.ttf \
		$PKG/usr/share/fonts/noto/
done

install -Dm644 sans/OFL.txt $PKG/usr/share/licenses/$name/OFL.txt

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
