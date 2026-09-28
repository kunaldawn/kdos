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

# Headers go to /usr/include/PySide6 and /usr/include/shiboken6, where the
# installed CMake packages point consumers; upstream's layout puts them under
# the prefix root beside the Python package.
patch -p1 -i "$PORT_SRC/fix-header-install-dir.patch"

# MODULES is the binding set, and naming it makes each one required: probed,
# a binding would appear or vanish with whatever Qt modules happened to be
# installed first. This set is what FreeCAD imports, plus the rest of qtbase.
# NO_QT_TOOLS keeps copies of Qt's own designer, uic and rcc out of the
# package; they are qt6-qttools' and qt6-qtbase's.
export LLVM_INSTALL_DIR=/usr
mkdir -p build && cd build
cmake .. -G Ninja -Wno-dev \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_SKIP_INSTALL_RPATH=ON \
	-DBUILD_SHARED_LIBS=ON \
	-DBUILD_TESTS=OFF \
	-DNO_QT_TOOLS=yes \
	-DPython_EXECUTABLE=/usr/bin/python3 \
	-DSHIBOKEN_PYTHON_LIBRARIES="$(pkg-config --libs python3-embed)" \
	-DMODULES="Core;Gui;Widgets;Network;Xml;Concurrent;DBus;Sql;Test;PrintSupport;OpenGL;OpenGLWidgets;Svg;SvgWidgets;UiTools"
PYTHONPATH="$PWD/sources" ninja
DESTDIR=$PKG ninja install

# The config written by the install step names the build tree's typesystem
# and glue directories; the build-tree copy names the installed ones.
install -Dm644 -t "$PKG/usr/lib/cmake/PySide6" \
	sources/pyside6/libpyside/PySide6Config.abi3.cmake

# pyside-tools installs its deployment scripts (deploy.py, project.py and the
# rest) straight into /usr/bin, where their names collide with anything; they
# are run through the PySide6 package instead.
site=$(python3 -c 'import sysconfig; print(sysconfig.get_path("platlib"))')
install -d "$PKG$site/PySide6/scripts"
for f in "$PKG"/usr/bin/*.py "$PKG"/usr/bin/*_lib "$PKG"/usr/bin/requirements-android.txt; do
	[ -e "$f" ] || continue
	mv "$f" "$PKG$site/PySide6/scripts/"
done
