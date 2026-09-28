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


# qdoc is off: it needs libclang, and it only writes the API reference, which
# no port builds. Everything a Qt 5 program or its build uses is on:
# Designer and its libraries (QtDesigner, QtUiTools), Linguist with lrelease
# and lupdate, Assistant with the QtHelp library, qdbus and qdbusviewer,
# qtpaths, qtdiag, qtplugininfo and pixeltool. No desktop entries are
# installed: these are developer tools, started by name.
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

# The tools stay in /usr/lib/qt5/bin, where Qt 5's CMake and pkg-config files
# point, and each gets a -qt5 name in /usr/bin; Qt 6 names its own with a 6.
install -d "$PKG/usr/bin"
for _tool in "$PKG"/usr/lib/qt5/bin/*; do
	ln -s "../lib/qt5/bin/${_tool##*/}" "$PKG/usr/bin/${_tool##*/}-qt5"
done
