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
	-feature-qml-jit \
	-feature-qml-network \
	-feature-qml-debug
make
make INSTALL_ROOT="$PKG" install

install -d "$PKG/usr/bin"
for _tool in "$PKG"/usr/lib/qt5/bin/*; do
	ln -s "../lib/qt5/bin/${_tool##*/}" "$PKG/usr/bin/${_tool##*/}-qt5"
done
