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

# The makefile asks `sdl2-config` and `libpng-config` for its flags, and the
# first does not exist here, so both answers come from pkgconf through the
# makefile's own variables. Its CFLAGS := -O2 would otherwise replace the
# build's flags, so they are passed the same way, with -fcommon: a header
# defines a global without `extern`, which links only as a common symbol.
#
# OpenGL links libOpenGL, the vendor-neutral library that needs no GLX: the
# context is SDL's, EGL on Wayland. Language files stay out (ENABLE_NLS=0).
# Only the programs and the compiled levels are built; the `desktops` target
# translates entries this recipe writes itself.
mk() {
	make \
		CFLAGS="$CFLAGS -fcommon" \
		SDL_CPPFLAGS="$(pkg-config --cflags sdl2)" \
		SDL_LIBS="$(pkg-config --libs sdl2)" \
		PNG_CPPFLAGS="$(pkg-config --cflags libpng)" \
		PNG_LIBS="$(pkg-config --libs libpng)" \
		OGL_LIBS=-lOpenGL \
		ENABLE_NLS=0 \
		DATADIR=/usr/share/neverball \
		"$@"
}
mk neverball neverputt mapc
mk sols

install -Dm755 neverball "$PKG/usr/bin/neverball"
install -Dm755 neverputt "$PKG/usr/bin/neverputt"
install -Dm755 mapc "$PKG/usr/bin/neverball-mapc"
install -Dm644 dist/neverball.6 "$PKG/usr/share/man/man6/neverball.6"
install -Dm644 dist/neverputt.6 "$PKG/usr/share/man/man6/neverputt.6"
install -Dm644 dist/mapc.1 "$PKG/usr/share/man/man1/neverball-mapc.1"

# The levels ship compiled; their .map sources are the input mapc just read.
install -d "$PKG/usr/share/neverball"
cp -R data/. "$PKG/usr/share/neverball/"
find "$PKG/usr/share/neverball" -name '*.map' -delete

for s in 16 24 32 48 64 128 256 512; do
	install -Dm644 dist/neverball_$s.png \
		"$PKG/usr/share/icons/hicolor/${s}x$s/apps/neverball.png"
	install -Dm644 dist/neverputt_$s.png \
		"$PKG/usr/share/icons/hicolor/${s}x$s/apps/neverputt.png"
done

# The Wayland app_id of each is SDL's default, the executable's name.
install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/neverball.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Neverball
GenericName=Ball-rolling Game
Comment=Tilt the floor to roll a ball through obstacle courses
Exec=neverball
Icon=neverball
Terminal=false
StartupWMClass=neverball
Categories=Game;ArcadeGame;
Keywords=ball;tilt;arcade;3d;neverball;
DESKTOP
cat > "$PKG/usr/share/applications/neverputt.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Neverputt
GenericName=Minigolf Game
Comment=Hot-seat minigolf for up to four players
Exec=neverputt
Icon=neverputt
Terminal=false
StartupWMClass=neverputt
Categories=Game;SportsGame;
Keywords=golf;minigolf;putt;neverputt;
DESKTOP
chmod 644 "$PKG/usr/share/applications/"*.desktop
