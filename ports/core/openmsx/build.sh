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

# The probe asks sdl2-config for SDL's flags and has no other route, and
# sdl2-config does not exist here. A build-local shim, first on PATH, answers
# it from pkgconf, which reads sdl2-compat's `Provides: sdl2`.
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
export PATH="$SRC_ROOT/sdl-shim:$PATH"

# The install directories are compiled into the binary, which reads its
# machines, extensions and scripts from the share directory, so the same
# values go to the build and to the install. The probe builds every
# component it finds the libraries for: the core, the OpenGL renderer
# (GLEW's GLX build; openMSX ignores the missing GLX display under
# Wayland), LaserDisc video (Ogg Theora and Vorbis) and ALSA MIDI. Each of
# those libraries is in depends so no component is quietly left out.
# C-BIOS, the free MSX system ROMs from Contrib, is installed, so the
# emulator starts with no copyrighted ROM.
_dirs=(
	INSTALL_BINARY_DIR=/usr/bin
	INSTALL_SHARE_DIR=/usr/share/openmsx
	INSTALL_DOC_DIR=/usr/share/doc/openmsx
	INSTALL_CONTRIB=true
	SYMLINK_FOR_BINARY=false
	OPENMSX_FLAVOUR=opt
	3RDPARTY_FLAG=false
)
./configure
make "${_dirs[@]}"
make install "${_dirs[@]}" DESTDIR="$PKG" INSTALL_VERBOSE=false

for _size in 16 32 48 64 128 256; do
	install -Dm644 "share/icons/openMSX-logo-$_size.png" \
		"$PKG/usr/share/icons/hicolor/${_size}x${_size}/apps/openmsx.png"
done

# Upstream ships no desktop entry. SDL's Wayland app_id is the executable's
# name.
install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/openmsx.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=openMSX
GenericName=MSX Emulator
Comment=Emulate the MSX family of home computers
TryExec=openmsx
Exec=openmsx %f
Icon=openmsx
Terminal=false
StartupWMClass=openmsx
Categories=Game;Emulator;
Keywords=msx;msx2;turbo r;emulator;retro;cbios;
DESKTOP
chmod 644 "$PKG/usr/share/applications/openmsx.desktop"
