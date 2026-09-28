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

# Both families compile from their FontForge sources. fontconfig's own
# 30-metric-aliases.conf maps Arial, Times New Roman, Courier New and Arial
# Narrow onto these names, so the port ships no configuration of its own.
make ttf-dir
make -C $SRC_ROOT/liberation-narrow-fonts-$_narrow ttf-dir

install -dm755 $PKG/usr/share/fonts/liberation
install -m644 liberation-fonts-ttf-$version/*.ttf $PKG/usr/share/fonts/liberation/
install -m644 $SRC_ROOT/liberation-narrow-fonts-$_narrow/liberation-narrow-fonts-ttf-$_narrow/*.ttf \
	$PKG/usr/share/fonts/liberation/

install -Dm644 LICENSE $PKG/usr/share/licenses/$name/LICENSE
install -Dm644 $SRC_ROOT/liberation-narrow-fonts-$_narrow/License.txt \
	$PKG/usr/share/licenses/$name/License-narrow.txt
install -Dm644 $SRC_ROOT/liberation-narrow-fonts-$_narrow/COPYING \
	$PKG/usr/share/licenses/$name/COPYING-narrow
