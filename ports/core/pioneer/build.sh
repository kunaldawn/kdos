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

# GLEW is the system's. Under Wayland glewInit() reports that no GLX display
# is current, and the renderer accepts exactly that answer when SDL's driver
# is wayland. Lua stays the bundled 5.2, the one version the scripts are
# written for and a version this tree does not carry.
#
# SSE4.2 is off: upstream turns it on for every x86 target, and a processor
# without it faults on the first such instruction instead of running slower.
# The developer key bindings are off.
mkdir build && cd build
cmake .. -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DPIONEER_DATA_DIR=/usr/share/pioneer/data \
	-DPIONEER_INSTALL_DATADIR=share/pioneer \
	-DPIONEER_INSTALL_BINDIR=bin \
	-DUSE_SYSTEM_LIBGLEW=ON \
	-DUSE_SYSTEM_LIBLUA=OFF \
	-DUSE_SSE42=OFF \
	-DUSE_AVX2=OFF \
	-DWITH_DEVKEYS=OFF \
	-DWITH_OBJECTVIEWER=ON \
	-DREMOTE_LUA_REPL=OFF \
	-DPROFILER_ENABLED=OFF \
	-DBUILD_WITH_OPENAL=ON
ninja

# The ship and station models ship in the optimised .sgm form, which the
# modelcompiler just built writes beside their sources. Install copies only
# that form, so an install without this step carries no models at all.
ninja build-data
DESTDIR=$PKG ninja install
cd ..

# English only: every other language's strings go.
find "$PKG/usr/share/pioneer/data/lang" -name '*.json' ! -name 'en.json' -delete

# Upstream's entry is replaced for StartupWMClass: the Wayland app_id is
# SDL's default, the executable's name. Its PNG icons are installed above.
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
