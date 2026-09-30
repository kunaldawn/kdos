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

# Both layouts are separate binaries, chosen at build time: mobintl for a
# phone-width screen, deskintl with the full desktop rows. The menu entry
# starts deskintl. The keyboard is a layer-shell surface, not a window, so the
# entry names no StartupWMClass; SIGUSR1 hides it, SIGUSR2 shows it and
# SIGRTMIN toggles it.
#
# config.mk assigns CFLAGS, and a makefile's assignment beats an exported
# variable, so the exported flags (-O2, the build-path map) would be dropped.
# They ride in CC instead, which every compile and link line names; CFLAGS on
# the command line would also discard the Makefile's own layout and
# pkg-config flags.
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
