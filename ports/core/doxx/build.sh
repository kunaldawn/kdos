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

export RUSTFLAGS="-C target-feature=-crt-static"
cargo build --release --frozen --offline
install -Dm755 target/release/doxx "$PKG/usr/bin/doxx"

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/doxx.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Word Document
GenericName=Document Viewer
Comment=Read a .docx in the terminal
Exec=doxx %f
Icon=x-office-document
Terminal=true
NoDisplay=true
MimeType=application/vnd.openxmlformats-officedocument.wordprocessingml.document;
Categories=Office;Viewer;
DESKTOP
chmod 644 "$PKG/usr/share/applications/doxx.desktop"
