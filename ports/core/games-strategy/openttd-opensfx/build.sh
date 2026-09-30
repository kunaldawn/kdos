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
# directory under baseset/ for a set's .obs index, so the set gets its own.
bsdtar -xf "$name-$version.zip"
tar -xf opensfx-$version.tar
cd opensfx-$version
install -d "$PKG/usr/share/games/openttd/baseset/opensfx"
install -m644 *.cat opensfx.obs license.txt readme.txt changelog.txt \
	"$PKG/usr/share/games/openttd/baseset/opensfx/"
