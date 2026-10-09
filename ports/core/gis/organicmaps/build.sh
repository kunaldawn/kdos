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

# WITH_SYSTEM_PROVIDED_3PARTY builds against the tree's boost, expat, gflags,
# jansson, pugixml, utfcpp, glm, fast_double_parser, ICU, FreeType and
# HarfBuzz. The release archive carries its submodules empty; three have no
# system form here and are later sources at the commits the tag pins: the
# project's own protobuf fork, just_gtfs and fast_obj. The patches are
# Alpine's: jansson through pkg-config (its autotools install has no CMake
# package), the fork of protobuf built even with system libraries, and the
# developer sandbox, glfw, imgui, glaze and the macOS deploy step left out.
# version-from-environment.patch is this recipe's own: an archive has no git
# history, so tools/unix/version.sh would otherwise make the build date the
# version the app reports; it takes the release's date and count from the tag.
# skip-search-test-support is too: the search test support and quality
# libraries are built for the desktop with the tests and tools left out, and
# they link test libraries that are then never defined.
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

# SKIP_TESTS leaves out the unit tests, which want the googletest submodule.
# SKIP_TOOLS leaves out the map generator's tools and the developer tools,
# none of which is installed; with the tests gone some would not link, as they
# link the test support libraries.
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
	-DUSE_PCH=OFF
cmake --build build
DESTDIR=$PKG cmake --install build
rm -rf "$PKG/usr/share/organicmaps/data/test_data"
[ -f "$PKG/usr/share/icons/hicolor/512x512/apps/organicmaps.png" ] ||
	{ echo "organicmaps: the hicolor PNG icon was not installed" >&2; exit 1; }

# The data directory carries World.mwm and WorldCoasts.mwm, the whole world
# at low zoom. Detailed regions are .mwm files of the same data version,
# which the in-app downloader fetches over the network; offline they are
# placed in ~/.local/share/OMaps. setDesktopFileName makes the Wayland app_id
# app.organicmaps.desktop, which the entry names so the panel ties the window
# to its launcher.
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
