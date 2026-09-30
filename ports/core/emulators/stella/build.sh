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

# configure asks sdl2-config for SDL's flags and has no other route, and
# sdl2-config does not exist here. A build-local shim answers it from pkgconf,
# which reads sdl2-compat's `Provides: sdl2`.
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

# System zlib, libpng and SQLite (the settings and ROM-properties store);
# each has a bundled copy that configure falls back to when the system one
# is missing. PlusROM cartridges reach a high-score server over HTTP from
# inside the game; on a machine with no network the cartridge runs and its
# upload fails, and nothing else in Stella goes online.
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

# UPSTREAM'S ENTRY IS REPLACED for StartupWMClass: SDL's Wayland app_id is
# the executable's name, stella.
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
