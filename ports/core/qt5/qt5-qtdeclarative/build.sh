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


# The QML JIT and the debugging and profiling hooks are built; python3 runs
# the build's own code generators, and nothing here needs it at run time.
/usr/lib/qt5/bin/qmake -- \
	-feature-qml-jit \
	-feature-qml-network \
	-feature-qml-debug
make
make INSTALL_ROOT="$PKG" install

# The tools stay in /usr/lib/qt5/bin, where Qt 5's CMake and pkg-config files
# point, and each gets a -qt5 name in /usr/bin; Qt 6 names its own with a 6.
install -d "$PKG/usr/bin"
for _tool in "$PKG"/usr/lib/qt5/bin/*; do
	ln -s "../lib/qt5/bin/${_tool##*/}" "$PKG/usr/bin/${_tool##*/}-qt5"
done
