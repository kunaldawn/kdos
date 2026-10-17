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

rmdir submodules/sanitizers-cmake submodules/mimalloc libraries/MCAD
mv "$SRC_ROOT/sanitizers-cmake-$_sancommit" submodules/sanitizers-cmake
mv "$SRC_ROOT/mimalloc-$_micommit" submodules/mimalloc
mv "$SRC_ROOT/MCAD-$_mcadcommit" libraries/MCAD

mkdir -p build && cd build
cmake .. -G Ninja -Wno-dev -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib \
	-DOPENSCAD_VERSION=$version \
	-DOPENSCAD_COMMIT=${_commit:0:9} \
	-DUSE_QT6=ON \
	-DHEADLESS=OFF \
	-DEXPERIMENTAL=OFF \
	-DENABLE_TESTS=OFF \
	-DENABLE_GUI_TESTS=OFF \
	-DENABLE_PYTHON=OFF \
	-DOFFLINE_DOCS=OFF \
	-DUSE_CCACHE=OFF \
	-DUSE_GLAD=ON \
	-DUSE_GLEW=OFF \
	-DENABLE_EGL=ON \
	-DENABLE_GLX=ON \
	-DENABLE_CGAL=ON \
	-DENABLE_MANIFOLD=ON \
	-DUSE_BUILTIN_MANIFOLD=OFF \
	-DUSE_BUILTIN_CLIPPER2=OFF \
	-DUSE_BUILTIN_OPENCSG=OFF \
	-DUSE_MIMALLOC=ON \
	-DENABLE_CAIRO=ON \
	-DENABLE_HIDAPI=ON \
	-DHIDAPI_INCLUDE_DIR="$(pkg-config --variable=includedir hidapi-hidraw)/hidapi" \
	-DHIDAPI_LIBRARY="$(pkg-config --variable=libdir hidapi-hidraw)/libhidapi-hidraw.so" \
	-DENABLE_SPNAV=ON \
	-DENABLE_QTDBUS=ON \
	-DENABLE_GAMEPAD=OFF \
	-DCMAKE_REQUIRE_FIND_PACKAGE_Lib3MF=ON
ninja
DESTDIR=$PKG ninja install

rm -rf "$PKG/usr/share/openscad/locale"
rm -rf "$PKG/usr/share/openscad/fonts/Liberation-2.00.1"

cat > "$PKG/usr/share/applications/openscad.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=OpenSCAD
GenericName=Script-based 3D CAD
Comment=Write solid 3D models as code and export STL, 3MF, OFF, DXF and SVG
Exec=openscad %f
Icon=openscad
Terminal=false
StartupWMClass=openscad
MimeType=application/x-openscad;
Categories=Graphics;3DGraphics;Engineering;Development;Qt;
Keywords=cad;3d;csg;script;stl;solid;model;print;openscad;
DESKTOP
chmod 644 "$PKG/usr/share/applications/openscad.desktop"
