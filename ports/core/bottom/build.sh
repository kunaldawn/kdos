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

tar xf $PORT_SRC/${name}-vendor-${version}.tar.xz

BTM_GENERATE=true cargo build --release --frozen --offline
install -Dm755 target/release/btm $PKG/usr/bin/btm
install -Dm644 target/tmp/bottom/manpage/btm.1 -t $PKG/usr/share/man/man1

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/btm.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Resource Monitor
GenericName=Resource Monitor
Comment=Graphs of CPU, memory, network and disks
Exec=btm
Icon=speedometer
Terminal=true
Categories=System;Monitor;
Keywords=process;cpu;memory;graph;bottom;btm;
EOF
chmod 644 "$PKG/usr/share/applications/btm.desktop"
