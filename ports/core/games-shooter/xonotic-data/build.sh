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

# The release archive also carries prebuilt engines for Linux, Windows and
# macOS, and the source the xonotic port builds from. Only the data
# packages, the release's player-ID key and the sample server configuration
# are taken, into the base directory the engine is compiled with.
unzip -q "$name-$version.zip" \
	'Xonotic/data/*' Xonotic/key_0.d0pk Xonotic/server/server.cfg \
	Xonotic/COPYING Xonotic/GPL-2 Xonotic/GPL-3
install -d "$PKG/usr/share/xonotic/data"
install -m644 Xonotic/data/*.pk3 "$PKG/usr/share/xonotic/data/"
install -m644 Xonotic/key_0.d0pk "$PKG/usr/share/xonotic/key_0.d0pk"
install -Dm644 Xonotic/server/server.cfg \
	"$PKG/usr/share/doc/xonotic/server.cfg"
install -Dm644 Xonotic/COPYING "$PKG/usr/share/licenses/xonotic-data/COPYING"
install -m644 Xonotic/GPL-2 Xonotic/GPL-3 "$PKG/usr/share/licenses/xonotic-data/"
