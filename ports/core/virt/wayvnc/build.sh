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
	--prefix=/usr --libdir=lib --sysconfdir=/etc \
	--buildtype=release \
	-Dscreencopy-dmabuf=enabled \
	-Dpam=enabled \
	-Dman-pages=enabled \
	-Dtests=false
meson compile -C build
DESTDIR=$PKG meson install --no-rebuild -C build

install -d -m 755 "$PKG/etc/pam.d"
cat > "$PKG/etc/pam.d/wayvnc" << "EOF"
auth    required pam_unix.so nodelay
account required pam_unix.so
EOF
chmod 644 "$PKG/etc/pam.d/wayvnc"
