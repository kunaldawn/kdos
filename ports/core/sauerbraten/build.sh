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

# The release archive carries the source, the game data and prebuilt
# engines under bin_unix/; the engines are built here instead and the
# prebuilt ones are never installed.
#
# The makefile asks `sdl2-config`, which does not exist here, inside its
# client include and library lines, so both are given whole from pkgconf.
# The bundled ENet is configured and linked statically by the makefile.
# The makefile assigns its own CXXFLAGS, which beats the environment, so the
# build's flags are passed ahead of upstream's optimisation flags: without
# them the file-prefix map is lost and the binary records the build path.
cd src
make \
	CXXFLAGS="$CXXFLAGS -O3 -fomit-frame-pointer -ffast-math" \
	CLIENT_INCLUDES="-Ishared -Iengine -Ifpsgame -Ienet/include $(pkg-config --cflags sdl2)" \
	CLIENT_LIBS="-Lenet -lenet -lX11 $(pkg-config --libs sdl2) -lSDL2_image -lSDL2_mixer -lz -lGL -lrt" \
	client server
install -Dm755 sauer_client "$PKG/usr/lib/sauerbraten/sauer_client"
install -Dm755 sauer_server "$PKG/usr/lib/sauerbraten/sauer_server"
cd ..

install -d "$PKG/usr/share/sauerbraten"
cp -R data packages server-init.cfg "$PKG/usr/share/sauerbraten/"
install -d "$PKG/usr/share/doc/sauerbraten"
cp -R docs README.html "$PKG/usr/share/doc/sauerbraten/"
install -Dm644 data/cube.png \
	"$PKG/usr/share/icons/hicolor/256x256/apps/sauerbraten.png"

# The engine reads its data from the working directory and writes the
# player's configuration and maps to the -q home directory, which is what
# upstream's sauerbraten_unix launcher does too.
install -d "$PKG/usr/bin"
cat > "$PKG/usr/bin/sauerbraten" <<'KDOS_SH'
#!/bin/sh
cd /usr/share/sauerbraten || exit 1
exec /usr/lib/sauerbraten/sauer_client "-q$HOME/.sauerbraten" "$@"
KDOS_SH
cat > "$PKG/usr/bin/sauerbraten-server" <<'KDOS_SH'
#!/bin/sh
cd /usr/share/sauerbraten || exit 1
exec /usr/lib/sauerbraten/sauer_server "-q$HOME/.sauerbraten" "$@"
KDOS_SH
chmod 755 "$PKG/usr/bin/sauerbraten" "$PKG/usr/bin/sauerbraten-server"

# The Wayland app_id is SDL's default, the executable's name: sauer_client.
install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/sauerbraten.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Cube 2: Sauerbraten
GenericName=First-Person Shooter
Comment=Fast shooter with co-operative in-game map editing
Exec=sauerbraten
Icon=sauerbraten
Terminal=false
StartupWMClass=sauer_client
Categories=Game;ActionGame;
Keywords=fps;shooter;cube;sauerbraten;editor;
DESKTOP
chmod 644 "$PKG/usr/share/applications/sauerbraten.desktop"
