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
export PATH="$SRC_ROOT/sdl-shim:$PATH"

_dirs=(
	INSTALL_BINARY_DIR=/usr/bin
	INSTALL_SHARE_DIR=/usr/share/openmsx
	INSTALL_DOC_DIR=/usr/share/doc/openmsx
	INSTALL_CONTRIB=true
	SYMLINK_FOR_BINARY=false
	OPENMSX_FLAVOUR=opt
	3RDPARTY_FLAG=false
	CXXFLAGS="${CXXFLAGS/-O2/-O3} -DNDEBUG"
	LDFLAGS="${LDFLAGS//-Wl,/}"
)
./configure
make "${_dirs[@]}"
make install "${_dirs[@]}" DESTDIR="$PKG" INSTALL_VERBOSE=false

for _size in 16 32 48 64 128 256; do
	install -Dm644 "share/icons/openMSX-logo-$_size.png" \
		"$PKG/usr/share/icons/hicolor/${_size}x${_size}/apps/openmsx.png"
done

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
