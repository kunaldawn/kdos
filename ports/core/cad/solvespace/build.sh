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

patch -p1 -i "$PORT_SRC/eigen5-cxx14.patch"

mkdir -p build && cd build
cmake .. -G Ninja -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
	-DENABLE_GUI=ON \
	-DUSE_QT_GUI=ON \
	-DENABLE_CLI=ON \
	-DENABLE_TESTS=OFF \
	-DENABLE_OPENMP=ON \
	-DENABLE_LTO=OFF \
	-DFORCE_VENDORED_Eigen3=OFF \
	-DEIGEN3_INCLUDE_DIRS=/usr/include/eigen3 \
	-DCMAKE_DISABLE_FIND_PACKAGE_SpaceWare=ON
ninja
DESTDIR=$PKG ninja install

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
