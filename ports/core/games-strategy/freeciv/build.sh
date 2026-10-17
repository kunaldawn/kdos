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

meson setup build --prefix=/usr --sysconfdir=/etc --libdir=lib \
	--buildtype=release \
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
for m in gtk3.22 gtk4 qt sdl3 modpack mp-cli mp-gtk3 mp-gtk4 mp-qt ruledit; do
	rm -f "$PKG/usr/share/man/man6/freeciv-$m.6"
done

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
desktop-file-edit --set-key=NoDisplay --set-value=true "$PKG/usr/share/applications/org.freeciv.server.desktop"
