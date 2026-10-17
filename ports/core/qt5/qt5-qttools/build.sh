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

/usr/lib/qt5/bin/qmake -- \
	-no-feature-qdoc \
	-feature-assistant \
	-feature-designer \
	-feature-linguist \
	-feature-qdbus \
	-feature-qtpaths \
	-feature-qtdiag \
	-feature-qtplugininfo \
	-feature-pixeltool
make
make INSTALL_ROOT="$PKG" install

install -d "$PKG/usr/bin"
for _tool in "$PKG"/usr/lib/qt5/bin/*; do
	ln -s "../lib/qt5/bin/${_tool##*/}" "$PKG/usr/bin/${_tool##*/}-qt5"
done
