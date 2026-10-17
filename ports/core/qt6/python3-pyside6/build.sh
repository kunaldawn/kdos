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

patch -p1 -i "$PORT_SRC/fix-header-install-dir.patch"

patch -p1 -i "$PORT_SRC/pyi-build-tree-shiboken.patch"
patch -p1 -i "$PORT_SRC/pc-dirs.patch"

export LLVM_INSTALL_DIR=/usr
_numpy=$(python3 -c 'import numpy; print(numpy.get_include())')
mkdir -p build && cd build
cmake .. -G Ninja -Wno-dev \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_SKIP_INSTALL_RPATH=ON \
	-DBUILD_SHARED_LIBS=ON \
	-DBUILD_TESTS=OFF \
	-DNO_QT_TOOLS=yes \
	-DPython_EXECUTABLE=/usr/bin/python3 \
	-DNUMPY_INCLUDE_DIR="$_numpy" \
	-DSHIBOKEN_PYTHON_LIBRARIES="$(pkg-config --libs python3-embed)" \
	-DMODULES="Core;Gui;Widgets;Network;Xml;Concurrent;DBus;Sql;Test;PrintSupport;OpenGL;OpenGLWidgets;Svg;SvgWidgets;UiTools;WebChannel;WebEngineCore;WebEngineWidgets"
PYTHONPATH="$PWD/sources" ninja
DESTDIR=$PKG ninja install

install -Dm644 -t "$PKG/usr/lib/cmake/PySide6" \
	sources/pyside6/libpyside/PySide6Config.abi3.cmake

site=$(python3 -c 'import sysconfig; print(sysconfig.get_path("platlib"))')
install -d "$PKG$site/PySide6/scripts"
for f in "$PKG"/usr/bin/*.py "$PKG"/usr/bin/*_lib "$PKG"/usr/bin/requirements-android.txt; do
	[ -e "$f" ] || continue
	mv "$f" "$PKG$site/PySide6/scripts/"
done
