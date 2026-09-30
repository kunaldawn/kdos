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

# Only the data directory is compiled into the program. The base directory
# receives VERSION, COPYING, CREDITS and ChangeLog, so it is the package's
# documentation directory rather than upstream's /usr.
#
# gettext's libintl.h, which i18n.h includes when it exists, maps its calls
# to libintl_* symbols that musl does not carry; -lintl at the end of every
# link line resolves them.
#
# GL is GLVND with GLEW. Under Wayland glewInit() reports that no GLX display
# is current, and the program accepts exactly that answer when SDL's video
# driver is wayland.
#
# The icon cache is left to kpkg's shared index: the install step would
# otherwise run gtk-update-icon-cache against the build root's own hicolor.
mkdir build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_CXX_STANDARD_LIBRARIES=-lintl \
	-DWL_INSTALL_BASEDIR=/usr/share/doc/widelands \
	-DWL_INSTALL_DATADIR=/usr/share/widelands \
	-DWL_INSTALL_BINDIR=/usr/bin \
	-DOPTION_BUILD_TESTS=OFF \
	-DOPTION_BUILD_CODECHECK=OFF \
	-DOPTION_BUILD_WEBSITE_TOOLS=OFF \
	-DOPTION_USE_GLBINDING=OFF \
	-DOPTION_GLEW_STATIC=OFF \
	-DOPTION_FORCE_EMBEDDED_MINIZIP=OFF \
	-DOPTION_ASAN=OFF \
	-DUSE_XDG=ON \
	-DUSE_FLTO_IF_AVAILABLE=no \
	-DOpenGL_GL_PREFERENCE=GLVND \
	-DGTK_UPDATE_ICON_CACHE=GTK_UPDATE_ICON_CACHE-NOTFOUND
ninja
DESTDIR=$PKG ninja install
cd ..
