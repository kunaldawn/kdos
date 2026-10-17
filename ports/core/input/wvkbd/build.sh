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

export LDFLAGS="$LDFLAGS -Wl,--strip-debug"
for layout in mobintl deskintl; do
	make LAYOUT=$layout CC="${CC:-gcc} $CFLAGS"
	make LAYOUT=$layout CC="${CC:-gcc} $CFLAGS" PREFIX=/usr DESTDIR=$PKG install
done

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/wvkbd.desktop" <<'EOF2'
[Desktop Entry]
Type=Application
Name=On-screen Keyboard
GenericName=Virtual Keyboard
Comment=Type with the pointer or a touch screen
Exec=wvkbd-deskintl
Icon=input-keyboard
Terminal=false
Categories=Utility;Accessibility;
Keywords=keyboard;osk;virtual;touch;onscreen;wvkbd;
EOF2
chmod 644 "$PKG/usr/share/applications/wvkbd.desktop"
