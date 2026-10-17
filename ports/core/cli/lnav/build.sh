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

tar xf $PORT_SRC/${name}-vendor-${version}.tar.xz -C src
export CARGO_HOME="$SRC_ROOT/.cargo"
export RUSTFLAGS="-C target-feature=-crt-static"
export CARGO_NET_OFFLINE=true

patch -p1 -i $PORT_SRC/lnav-tailer-static.patch

./autogen.sh

./configure --prefix=/usr --sysconfdir=/etc --disable-static \
	--with-libarchive \
	--with-cargo \
	--disable-system-paths \
	ac_cv_path_RE2C_CMD= \
	ac_cv_path_CXXBRIDGE=
make
make DESTDIR=$PKG install

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/lnav.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Log Viewer
GenericName=Log Viewer
Comment=Merge, filter and search log files
Exec=lnav %F
Icon=text-x-generic
Terminal=true
Categories=System;Monitor;Utility;
Keywords=log;journal;syslog;filter;lnav;
EOF
chmod 644 "$PKG/usr/share/applications/lnav.desktop"
