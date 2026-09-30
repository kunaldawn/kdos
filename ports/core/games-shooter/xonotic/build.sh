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

# The source release is a zip, copied here whole rather than unpacked.
unzip -q "$name-$version.zip"
cd Xonotic

# gcc-15.patch is Alpine's: C23 made bool, true and false keywords, and
# DarkPlaces defines its own.
patch -p1 -i "$PORT_SRC/gcc-15.patch"

# d0_blind_id is the player-ID and encryption library, big numbers on GMP.
# DarkPlaces dlopens both of its libraries at run time, so they are shared
# and installed beside the engine.
cd source/d0_blind_id
./configure --prefix=/usr --libdir=/usr/lib --disable-static
make
make DESTDIR=$PKG install
cd ../..

# The SDL client and the dedicated server. The data directory is compiled
# in; xonotic-data installs it. `sdl2-config` does not exist here, so SDL's
# flags come from pkgconf. The build flags travel in CPUOPTIMIZATIONS, the
# one variable the makefile adds to its own, and it comes last on the line, so
# its -O2 is raised to the -O3 of the makefile's release level. The makefile
# sets LDFLAGS itself and ignores the exported one, but its release link line
# repeats CPUOPTIMIZATIONS ahead of the libraries, so LDFLAGS rides there too;
# the compiler drops -Wl, options on a compile-only line. The exported
# -std=gnu11 is removed: gcc-15.patch leaves dpsoftrast.c using the C23 bool
# keyword, which gnu11 does not have. SDLCONFIG_UNIXLIBS_X11
# is emptied: only the GLX client calls Xlib, and the makefile would otherwise
# link libX11 into the SDL client too. zlib and libjpeg are linked;
# libpng, libogg, libvorbis, libtheora and libcurl are dlopened at run time,
# so they are dependencies no link line shows.
dpflags=${CFLAGS/-O2/-O3}
dpflags=${dpflags/-std=gnu11/}
for t in sdl-release sv-release; do
	make -C source/darkplaces \
		CPUOPTIMIZATIONS="$dpflags $LDFLAGS" \
		SDL_CONFIG='pkg-config sdl2' \
		SDLCONFIG_UNIXLIBS_X11= \
		DP_FS_BASEDIR=/usr/share/xonotic \
		$t
done
install -Dm755 source/darkplaces/darkplaces-sdl "$PKG/usr/bin/xonotic"
install -Dm755 source/darkplaces/darkplaces-dedicated "$PKG/usr/bin/xonotic-dedicated"

for s in 16 22 24 32 48 64 128 256 512; do
	install -Dm644 misc/logos/icons_png/xonotic_$s.png \
		"$PKG/usr/share/icons/hicolor/${s}x$s/apps/xonotic.png"
done

# DarkPlaces picks the game from its executable's name, which is why the
# client is installed as `xonotic`; the Wayland app_id is SDL's default,
# that same name.
install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/xonotic.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Xonotic
GenericName=First-Person Shooter
Comment=Fast arena shooter, for LAN and local play against bots
Exec=xonotic
Icon=xonotic
Terminal=false
StartupWMClass=xonotic
Categories=Game;ActionGame;
Keywords=fps;shooter;arena;deathmatch;xonotic;
DESKTOP
chmod 644 "$PKG/usr/share/applications/xonotic.desktop"
