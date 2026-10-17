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

meson setup build \
	--prefix=/usr --libdir=lib \
	--buildtype=release \
	-Dopencv=enabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

cat > "$PKG/usr/share/applications/wl-kbptr.desktop" <<'EOF2'
[Desktop Entry]
Type=Application
Name=Keyboard Pointer
GenericName=Pointer Control
Comment=Move and click the pointer from the keyboard
Exec=wl-kbptr
Icon=input-mouse
Terminal=false
Categories=Utility;Accessibility;
Keywords=mouse;pointer;keyboard;click;accessibility;kbptr;
EOF2
chmod 644 "$PKG/usr/share/applications/wl-kbptr.desktop"

install -Dm644 config.example "$PKG/usr/share/doc/wl-kbptr/config.example"
