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
install -Dm755 target/release/procs $PKG/usr/bin/procs

export XML_CATALOG_FILES=/etc/xml/catalog
a2x -f manpage -D man man/procs.1.adoc
install -Dm644 man/procs.1 -t "$PKG/usr/share/man/man1"

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/procs.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Processes
GenericName=Process List
Comment=A process table you can search
Exec=procs
Icon=system-run
Terminal=true
Categories=System;Monitor;
Keywords=process;ps;search;procs;
EOF
chmod 644 "$PKG/usr/share/applications/procs.desktop"
