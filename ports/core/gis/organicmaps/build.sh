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

for p in use-external-jansson libs-hack use-internal-protobuf \
	no-dev-sandbox no-macdeployqt version-from-environment \
	skip-search-test-support; do
	patch -p1 -i "$PORT_SRC/$p.patch"
done
_om=${_tag%-linux}
export OM_VERSION_DATE=${_om%-*} OM_VERSION_COUNT=${_om##*-}
cp -a "$SRC_ROOT/just_gtfs-$_gtfs/." 3party/just_gtfs/
cp -a "$SRC_ROOT/protobuf-$_protobuf/." 3party/protobuf/protobuf/
cp -a "$SRC_ROOT/fast_obj-$_fastobj/." 3party/fast_obj/

cmake -S . -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DBUILD_SHARED_LIBS=OFF \
	-DWITH_SYSTEM_PROVIDED_3PARTY=ON \
	-DBUILD_DESIGNER=OFF \
	-DBUILD_STANDALONE=ON \
	-DSKIP_TESTS=ON \
	-DSKIP_TOOLS=ON \
	-DSKIP_QT_GUI=OFF \
	-DPYBINDINGS=OFF \
	-DUSE_CCACHE=OFF \
	-DUSE_PCH=OFF \
	-DCMAKE_EXE_LINKER_FLAGS_RELEASE="$LDFLAGS"
cmake --build build
DESTDIR=$PKG cmake --install build
rm -rf "$PKG/usr/share/organicmaps/data/test_data"
[ -f "$PKG/usr/share/icons/hicolor/512x512/apps/organicmaps.png" ] ||
	{ echo "organicmaps: the hicolor PNG icon was not installed" >&2; exit 1; }

cat > "$PKG/usr/share/applications/app.organicmaps.desktop.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Organic Maps
GenericName=Offline Maps
Comment=Offline maps with search and routing, from OpenStreetMap
Exec=OMaps
TryExec=OMaps
Icon=organicmaps
Terminal=false
StartupWMClass=app.organicmaps.desktop
Categories=Qt;Education;Geography;Maps;
Keywords=map;maps;offline;navigation;route;osm;openstreetmap;organic;
DESKTOP
chmod 644 "$PKG/usr/share/applications/app.organicmaps.desktop.desktop"
