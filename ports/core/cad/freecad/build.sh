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

# THE RELEASE TARBALL IS USED, NOT THE TAG ARCHIVE: it carries the
# OndselSolver, GSL and AddonManager submodules, which a tag archive leaves
# empty. It is FLAT, with no wrapping directory, and kpkg strips one
# component from the first source, which discards every top-level file; it
# is unpacked again here, unstripped.
mkdir -p unpacked
tar xf "$PORT_SRC/$name-$version.tar.gz" -C unpacked
cd unpacked

# musl has no execinfo.h: the crash handler and SMESH's trace print no
# backtrace. UnlimitedUnsigned.h uses the fixed-width types without
# including <cstdint>, which libstdc++ no longer includes transitively.
patch -p1 -i "$PORT_SRC/no-execinfo.patch"
patch -p1 -i "$PORT_SRC/UnlimitedUnsigned.h-cstdint.patch"

# FreeCAD finds its modules beside its executable (AppHomePath/Mod), so the
# program tree lives under /usr/lib/freecad and only the launchers are
# linked into /usr/bin. Data, documentation and the desktop files take
# absolute paths, which FreeCAD resolves as given. The bin directory is
# absolute too: the thumbnailer entry names the program by it, and a
# relative one would be written into that entry as bin/freecad-thumbnailer.
#
# BUILD_ADDONMGR=OFF: the Addon Manager is a front end to an online catalogue
# and does nothing offline. PCL is not a port. FEM meshes with Netgen and
# solves with CalculiX, run as ccx from PATH; BIM imports and exports IFC
# through ifcopenshell. 3D mice are read through libspnav from spacenavd's
# socket; spacenavd is not a port, so no device reaches it. MED and VTK are
# required, not optional: MeshPart and FEM build the bundled SMESH, which
# needs both. APPDATA_RELEASE_DATE is the release's date; unset, the
# metainfo file records the day of the build.
_prefix=/usr/lib/freecad
mkdir -p build && cd build
cmake .. -G Ninja -Wno-dev -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=$_prefix \
	-DCMAKE_INSTALL_BINDIR=$_prefix/bin \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DCMAKE_INSTALL_INCLUDEDIR=include \
	-DCMAKE_INSTALL_DATAROOTDIR=/usr/share \
	-DCMAKE_INSTALL_DATADIR=/usr/share/freecad \
	-DCMAKE_INSTALL_DOCDIR=/usr/share/doc/freecad \
	-DPython3_EXECUTABLE=/usr/bin/python3 \
	-DFREECAD_QT_VERSION=6 \
	-DBUILD_GUI=ON \
	-DBUILD_TEST=OFF \
	-DENABLE_DEVELOPER_TESTS=OFF \
	-DBUILD_ADDONMGR=OFF \
	-DBUILD_FEM=ON \
	-DBUILD_FEM_NETGEN=ON \
	-DBUILD_DESIGNER_PLUGIN=OFF \
	-DFREECAD_USE_PCL=OFF \
	-DFREECAD_USE_3DCONNEXION_LEGACY=ON \
	-DFREECAD_USE_FREETYPE=ON \
	-DFREECAD_USE_EXTERNAL_FMT=ON \
	-DFREECAD_USE_EXTERNAL_SMESH=OFF \
	-DFREECAD_USE_PYSIDE=ON \
	-DFREECAD_USE_SHIBOKEN=ON \
	-DFREECAD_CHECK_PIVY=ON \
	-DAPPDATA_RELEASE_DATE=$_date
ninja
DESTDIR=$PKG ninja install

install -d "$PKG/usr/bin"
for b in FreeCAD FreeCADCmd freecad-thumbnailer; do
	ln -s "../lib/freecad/bin/$b" "$PKG/usr/bin/$b"
done
ln -s FreeCAD "$PKG/usr/bin/freecad"
ln -s FreeCADCmd "$PKG/usr/bin/freecadcmd"

# UPSTREAM'S ENTRY IS REPLACED: its StartupWMClass is the X11 class, while
# main() sets the desktop file name, so the Wayland app_id is
# org.freecad.FreeCAD. It claims only FreeCAD's own document type; the mesh
# and exchange formats it also opens belong to the viewers. The hicolor PNGs
# (16 to 64 px) are upstream's.
cat > "$PKG/usr/share/applications/org.freecad.FreeCAD.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=FreeCAD
GenericName=Parametric CAD Modeller
Comment=Design parts and assemblies, drawings, FEM and CAM from parametric 3D models
Exec=FreeCAD --single-instance %F
Icon=org.freecad.FreeCAD
Terminal=false
StartupNotify=true
StartupWMClass=org.freecad.FreeCAD
MimeType=application/x-extension-fcstd;
Categories=Graphics;Science;Engineering;3DGraphics;Qt;
Keywords=cad;3d;parametric;modeling;sketch;step;stl;fem;cam;bim;freecad;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.freecad.FreeCAD.desktop"
