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

patch -p1 -i "$PORT_SRC/musl-select.patch"
patch -p1 -i "$PORT_SRC/cstdint.patch"

mkdir build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DPIONEER_DATA_DIR=/usr/share/pioneer/data \
	-DPIONEER_INSTALL_DATADIR=share/pioneer \
	-DPIONEER_INSTALL_BINDIR=bin \
	-DUSE_SYSTEM_LIBGLEW=ON \
	-DCMAKE_IGNORE_PATH='/usr/lib/cmake/glew;/lib/cmake/glew' \
	-DUSE_SYSTEM_LIBLUA=OFF \
	-DUSE_SSE42=OFF \
	-DUSE_AVX2=OFF \
	-DWITH_DEVKEYS=OFF \
	-DWITH_OBJECTVIEWER=ON \
	-DREMOTE_LUA_REPL=OFF \
	-DPROFILER_ENABLED=OFF \
	-DBUILD_WITH_OPENAL=ON
ninja

ninja build-data
DESTDIR=$PKG ninja install
cd ..
for _t in editor modelcompiler savegamedump; do
	mv "$PKG/usr/bin/$_t" "$PKG/usr/bin/pioneer-$_t"
done

find "$PKG/usr/share/pioneer/data/lang" -name '*.json' ! -name 'en.json' -delete

rm -f "$PKG/usr/share/applications/net.pioneerspacesim.Pioneer.desktop"
cat > "$PKG/usr/share/applications/net.pioneerspacesim.Pioneer.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Pioneer
GenericName=Space Simulator
Comment=Trade, fight and explore across the Milky Way
Exec=pioneer
Icon=net.pioneerspacesim.Pioneer
Terminal=false
StartupWMClass=pioneer
Categories=Game;Simulation;
Keywords=space;trading;elite;simulator;pioneer;
DESKTOP
chmod 644 "$PKG/usr/share/applications/net.pioneerspacesim.Pioneer.desktop"
