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

# THE RELEASE TARBALL IS USED, NOT THE TAG ARCHIVE: it carries libdxfrw and
# mimalloc, submodules a tag archive leaves empty. Both are linked statically
# and neither is a port. Every other library under extlib/ is taken from the
# system, so no FORCE_VENDORED switch is set.
#
# USE_QT_GUI builds solvespace-qt on Qt 6 OpenGLWidgets; the GTK interface
# needs gtkmm and x11. The command-line tool solvespace-cli (exports and the
# file thumbnailer) needs cairo. The 3D-mouse support (libspnav) lives only
# in the GTK and Windows interfaces; solvespace-qt has none, so the lookup is
# disabled and the library, which would be linked and never called, is not.
mkdir -p build && cd build
cmake .. -G Ninja -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
	-DENABLE_GUI=ON \
	-DUSE_QT_GUI=ON \
	-DENABLE_CLI=ON \
	-DENABLE_TESTS=OFF \
	-DENABLE_OPENMP=OFF \
	-DENABLE_LTO=OFF \
	-DFORCE_VENDORED_Eigen3=OFF \
	-DCMAKE_DISABLE_FIND_PACKAGE_SpaceWare=ON
ninja
DESTDIR=$PKG ninja install

# UPSTREAM'S ENTRY IS REPLACED for StartupWMClass: nothing sets a desktop
# file name, so Qt takes the Wayland app_id from the program name,
# solvespace-qt. The hicolor PNGs (16 to 48 px) are upstream's.
cat > "$PKG/usr/share/applications/solvespace-qt.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=SolveSpace
GenericName=Parametric CAD
Comment=Constraint-based 2D sketches and 3D parts
Exec=solvespace-qt %f
Icon=solvespace
Terminal=false
StartupWMClass=solvespace-qt
MimeType=application/x-solvespace;
Categories=Graphics;3DGraphics;Engineering;Qt;
Keywords=parametric;cad;2d;3d;sketch;constraint;dxf;step;stl;solvespace;
DESKTOP
chmod 644 "$PKG/usr/share/applications/solvespace-qt.desktop"
