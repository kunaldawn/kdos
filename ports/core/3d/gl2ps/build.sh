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

# gl2ps reads OpenGL feedback buffers into PostScript, EPS, PDF and SVG: it is
# how Octave's Qt figures print to vector files, and Octave's configure drops
# printing when the library is absent. GLUT only builds two demo programs and
# LaTeX only the manual, so neither is looked for. The static archive upstream
# always builds is not shipped.
cmake -S . -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DENABLE_ZLIB=ON \
	-DENABLE_PNG=ON \
	-DOpenGL_GL_PREFERENCE=GLVND \
	-DCMAKE_DISABLE_FIND_PACKAGE_GLUT=ON \
	-DCMAKE_DISABLE_FIND_PACKAGE_LATEX=ON
cmake --build build
DESTDIR=$PKG cmake --install build
rm -f "$PKG"/usr/lib/libgl2ps.a
test -e "$PKG"/usr/lib/libgl2ps.so
