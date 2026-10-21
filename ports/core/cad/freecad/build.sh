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

mkdir -p unpacked
tar xf "$PORT_SRC/$name-$version.tar.gz" -C unpacked
cd unpacked

patch -p1 -i "$PORT_SRC/no-execinfo.patch"
patch -p1 -i "$PORT_SRC/UnlimitedUnsigned.h-cstdint.patch"
patch -p1 -i "$PORT_SRC/pyside612-pyobjectwrapper.patch"

_coin=$(pkg-config --modversion Coin)
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
	-Dpybind11_DIR="$(python3 -m pybind11 --cmakedir)" \
	-DCOIN3D_VERSION="$_coin" \
	-DCOIN3D_MAJOR_VERSION="${_coin%%.*}" \
	-DCOIN3D_MINOR_VERSION="$(echo "$_coin" | cut -d. -f2)" \
	-DCOIN3D_MICRO_VERSION="${_coin##*.}" \
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
