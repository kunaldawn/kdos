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

cmake -S . -B build -G Ninja \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DCMAKE_INSTALL_LIBDIR=lib \
	-DENABLE_GAME=ON \
	-DENABLE_SERVER=ON \
	-DENABLE_CAMPAIGN_SERVER=OFF \
	-DENABLE_MYSQL=OFF \
	-DENABLE_TESTS=OFF \
	-DENABLE_NLS=OFF \
	-DENABLE_SYSTEM_LUA=OFF \
	-DENABLE_DISPLAY_REVISION=OFF \
	-DENABLE_DESKTOP_ENTRY=ON \
	-DENABLE_APPDATA_FILE=ON \
	-DENABLE_NOTIFICATIONS=ON \
	-DENABLE_STRICT_COMPILATION=OFF \
	-DENABLE_LTO=OFF \
	-DFIFO_DIR=/run/wesnothd
cmake --build build
DESTDIR=$PKG cmake --install build
rm -rf "$PKG/run"

cat > "$PKG/usr/share/applications/org.wesnoth.Wesnoth.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Battle for Wesnoth
GenericName=Strategy Game
Comment=Fantasy turn-based strategy: campaigns, skirmishes and a map editor
Exec=wesnoth
Icon=wesnoth-icon
Terminal=false
StartupWMClass=wesnoth
Categories=Game;StrategyGame;
Keywords=strategy;turn-based;fantasy;campaign;hex;wesnoth;
Actions=Editor;

[Desktop Action Editor]
Name=Map Editor
Exec=wesnoth -e
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.wesnoth.Wesnoth.desktop"
