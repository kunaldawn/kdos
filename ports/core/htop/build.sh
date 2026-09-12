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
./configure --prefix=/usr --disable-nls --mandir=/usr/share/man
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
