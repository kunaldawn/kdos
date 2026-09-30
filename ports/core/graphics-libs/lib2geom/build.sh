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

# The toys need GTK 3 and are excluded from the default target, so they are
# never built. The SVG path parser is shipped pre-generated, so ragel is not
# needed. GSL enables the fitting code Inkscape's path effects use.
mkdir build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-D2GEOM_BUILD_SHARED=ON \
	-D2GEOM_USE_GPL_CODE=ON \
	-D2GEOM_TESTING=OFF \
	-DWITH_PROFILING=OFF \
	-DWITH_COVERAGE=OFF
ninja
DESTDIR=$PKG ninja install
