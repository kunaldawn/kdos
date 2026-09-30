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

# The last release, 2021.01, is Qt 5 and predates CGAL 6 and Manifold; the
# port follows master at a pinned commit, which is how upstream ships its
# development snapshots.
#
# A commit archive leaves the submodules empty. Three are later sources:
# sanitizers-cmake (the build calls add_sanitizers() unconditionally),
# mimalloc (linked statically as the allocator; no port carries it) and the
# MCAD library installed beside the examples. Manifold, Clipper2 and OpenCSG
# are ports, so their submodules stay empty and the USE_BUILTIN_* switches
# are off.
rmdir submodules/sanitizers-cmake submodules/mimalloc libraries/MCAD
mv "$SRC_ROOT/sanitizers-cmake-$_sancommit" submodules/sanitizers-cmake
mv "$SRC_ROOT/mimalloc-$_micommit" submodules/mimalloc
mv "$SRC_ROOT/MCAD-$_mcadcommit" libraries/MCAD

# OPENSCAD_VERSION and OPENSCAD_COMMIT are given: with neither VERSION.txt
# nor a git checkout, the version would be the day of the build.
#
# GUI rendering: GLAD, with EGL and GLX both built for the offscreen
# (export and command-line) context. 3MF import and export link lib3mf, a
# required find, so a missing library fails configure rather than leaving
# the dummy 3MF sources in. ENABLE_SPNAV=ON builds the 3D-mouse input driver
# on libspnav, and fails configure when it is absent; libspnav reaches the
# device through the spacenavd daemon, which no port provides, so the driver
# finds no mouse. OFFLINE_DOCS downloads the manual at build time, and the
# Python interpreter is experimental; both are off.
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
	-DENABLE_SPNAV=ON \
	-DENABLE_QTDBUS=ON \
	-DENABLE_GAMEPAD=OFF \
	-DCMAKE_REQUIRE_FIND_PACKAGE_Lib3MF=ON
ninja
DESTDIR=$PKG ninja install

# The interface is English; the message catalogues are left out. The bundled
# Liberation faces are left out too: the font-liberation port installs the
# same families system-wide, and fontconfig resolves the aliases in
# fonts/10-liberation.conf to them.
rm -rf "$PKG/usr/share/openscad/locale"
rm -rf "$PKG/usr/share/openscad/fonts/Liberation-2.00.1"

# UPSTREAM'S ENTRY IS REPLACED for StartupWMClass: main() sets the desktop
# file name openscad, which is the Wayland app_id. The hicolor PNGs (48 to
# 512 px) are upstream's.
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
Categories=Graphics;3DGraphics;Engineering;Programming;Qt;
Keywords=cad;3d;csg;script;stl;solid;model;print;openscad;
DESKTOP
chmod 644 "$PKG/usr/share/applications/openscad.desktop"
