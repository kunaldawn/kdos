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
tar -xf opengfx-$version.tar
cd opengfx-$version
install -d "$PKG/usr/share/games/openttd/baseset/opengfx"
install -m644 *.grf opengfx.obg license.txt readme.txt changelog.txt \
	"$PKG/usr/share/games/openttd/baseset/opengfx/"
