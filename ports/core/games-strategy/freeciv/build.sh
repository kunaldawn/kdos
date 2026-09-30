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

# The SDL2 client, the server it starts for a local game, and the command-line
# rules tools. The modpack installer only downloads from the network, so none
# of its front ends is built; the rules editor is Qt and is left out. English
# only. Lua is the copy freeciv bundles, which its tolua bindings are built
# against.
meson setup build --prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release -Db_ndebug=if-release \
	-Dclients=sdl2 \
	-Dfcmp='[]' \
	-Dserver=enabled \
	-Daudio=sdl2 \
	-Dtools=manual,ruleup \
	-Dnls=false \
	-Dsyslua=false \
	-Dsys-tolua-cmd=false \
	-Dreadline=true \
	-Dmwand=false \
	-Djson-protocol=false \
	-Dgitrev=false \
	-Dappimage=false \
	-Dsvgflags=true
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

# The window's app_id is the program's name.
cat > "$PKG/usr/share/applications/org.freeciv.sdl2.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Freeciv
GenericName=Strategy Game
Comment=Turn-based strategy inspired by the history of human civilization
Exec=freeciv-sdl2
Icon=freeciv-client
Terminal=false
StartupWMClass=freeciv-sdl2
Categories=Game;StrategyGame;
Keywords=strategy;simulation;civilization;tiles;history;multiplayer;
DESKTOP
chmod 644 "$PKG/usr/share/applications/org.freeciv.sdl2.desktop"
