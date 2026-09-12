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

cargo build --release --frozen --offline
install -Dm755 target/release/dust $PKG/usr/bin/dust

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/dust.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Disk Usage (dust)
GenericName=Disk Usage Analyser
Comment=A tree of what is filling the disk
Exec=dust
Icon=drive-harddisk
Terminal=true
Categories=System;Filesystem;
Keywords=disk;usage;space;du;dust;
EOF
chmod 644 "$PKG/usr/share/applications/dust.desktop"
