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

bsdtar -xf "$name-$version.zip"
tar -xf opensfx-$version.tar
cd opensfx-$version
install -d "$PKG/usr/share/games/openttd/baseset/opensfx"
install -m644 *.cat opensfx.obs license.txt readme.txt changelog.txt \
	"$PKG/usr/share/games/openttd/baseset/opensfx/"
