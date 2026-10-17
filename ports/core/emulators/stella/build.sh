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

install -d "$SRC_ROOT/sdl-shim"
cat > "$SRC_ROOT/sdl-shim/sdl2-config" <<'SHIM'
#!/bin/sh
case "$1" in
--cflags) exec pkg-config --cflags sdl2 ;;
--libs) exec pkg-config --libs sdl2 ;;
--version) exec pkg-config --modversion sdl2 ;;
*) exit 1 ;;
esac
SHIM
chmod 755 "$SRC_ROOT/sdl-shim/sdl2-config"

./configure --prefix=/usr \
	--with-sdl-prefix="$SRC_ROOT/sdl-shim" \
	--enable-release \
	--enable-gui \
	--enable-sound \
	--enable-debugger \
	--enable-joystick \
	--enable-cheats \
	--enable-png \
	--enable-zip \
	--enable-windowed \
	--enable-shared
make
make DESTDIR=$PKG install

cat > "$PKG/usr/share/applications/stella.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Stella
GenericName=Atari 2600 Emulator
Comment=Emulate the Atari 2600 Video Computer System
TryExec=stella
Exec=stella %f
Icon=stella
Terminal=false
StartupWMClass=stella
Categories=Game;Emulator;
Keywords=atari;2600;vcs;emulator;retro;
DESKTOP
chmod 644 "$PKG/usr/share/applications/stella.desktop"
