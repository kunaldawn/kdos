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


# ENABLE_GUI=OFF: the editor window is GTK 3 and gtkmm, which stay off the
# host. What is left is the `fontforge` command running native (`-script`)
# and Python scripts, and the `fontforge` Python module the font ports import
# to compile faces from their drawing sources.
#
# Every optional library is named ON or OFF, because an AUTO option that
# misses its library builds a narrower fontforge without a word. libspiro and
# woff2 are not ports; harfbuzz is only the GUI's metrics-view shaper. The
# HTML manual needs Sphinx and is not built; the four manual pages install
# regardless.
mkdir build
cd build
cmake .. -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=ON -DBUILD_TESTING=OFF \
	-DENABLE_GUI=OFF \
	-DENABLE_NATIVE_SCRIPTING=ON \
	-DENABLE_PYTHON_SCRIPTING=ON \
	-DENABLE_PYTHON_EXTENSION=ON \
	-DENABLE_LIBGIF=ON \
	-DENABLE_LIBJPEG=ON \
	-DENABLE_LIBPNG=ON \
	-DENABLE_LIBTIFF=ON \
	-DENABLE_LIBREADLINE=ON \
	-DENABLE_LIBSPIRO=OFF \
	-DENABLE_WOFF2=OFF \
	-DENABLE_HARFBUZZ=OFF \
	-DENABLE_DOCS=OFF
make
make DESTDIR=$PKG install
