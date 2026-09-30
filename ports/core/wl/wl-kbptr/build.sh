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

# The overlay is a wlr-layer-shell surface and the click goes out through
# zwlr_virtual_pointer_v1, so both globals must be offered by the compositor
# or the program exits without drawing. opencv drives target detection:
# the floating mode's `detect` source reads the screen through
# wlr-screencopy and finds clickable regions with opencv's imgproc, and it
# links pixman for the capture buffer. The other modes do not use it.
meson setup build \
	--prefix=/usr --libdir=lib \
	--buildtype=release \
	-Dopencv=enabled
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

# Upstream's entry names the program rather than the role and no icon the
# panel can draw, so it is replaced. The menu row is the one start path: no
# key binding runs wl-kbptr.
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
