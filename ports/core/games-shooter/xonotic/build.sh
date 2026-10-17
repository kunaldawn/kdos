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

unzip -q "$name-$version.zip"
cd Xonotic

patch -p1 -i "$PORT_SRC/gcc-15.patch"

cd source/d0_blind_id
./configure --prefix=/usr --libdir=/usr/lib --disable-static
make
make DESTDIR=$PKG install
cd ../..

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
