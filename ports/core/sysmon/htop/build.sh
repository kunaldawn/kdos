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

./autogen.sh
./configure --prefix=/usr --sysconfdir=/etc --mandir=/usr/share/man \
	--enable-sensors \
	--enable-delayacct \
	--enable-capabilities \
	--enable-backtrace=unwind-ptrace \
	--with-libunwind=yes \
	--disable-demangling
make
make DESTDIR=$PKG install

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/htop.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Process Viewer
GenericName=Process Viewer
Comment=Watch and signal running processes
Exec=htop
Icon=speedometer
Terminal=true
Categories=System;Monitor;
Keywords=process;task;kill;monitor;htop;
EOF
chmod 644 "$PKG/usr/share/applications/htop.desktop"
