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

# The release zip holds one tarball of the finished set. OpenTTD looks in each
# directory under baseset/ for a set's .obg index, so the set gets its own.
bsdtar -xf "$name-$version.zip"
tar -xf opengfx-$version.tar
cd opengfx-$version
install -d "$PKG/usr/share/games/openttd/baseset/opengfx"
install -m644 *.grf opengfx.obg license.txt readme.txt changelog.txt \
	"$PKG/usr/share/games/openttd/baseset/opengfx/"
