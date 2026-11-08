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

unzip -q -o "$PORT_SRC/$name-$version.zip" -d jbm

install -dm755 $PKG/usr/share/fonts/TTF
install -m644 jbm/fonts/ttf/JetBrainsMono-*.ttf jbm/fonts/ttf/JetBrainsMonoNL-*.ttf \
	$PKG/usr/share/fonts/TTF/

install -Dm644 jbm/OFL.txt $PKG/usr/share/licenses/$name/OFL.txt
