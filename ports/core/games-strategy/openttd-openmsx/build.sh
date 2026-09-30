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
# directory under baseset/ for a set's .obm index, so the set gets its own.
bsdtar -xf "$name-$version.zip"
tar -xf openmsx-$version.tar
cd openmsx-$version
install -d "$PKG/usr/share/games/openttd/baseset/openmsx"
install -m644 *.mid openmsx.obm license.txt readme.txt changelog.txt \
	"$PKG/usr/share/games/openttd/baseset/openmsx/"
