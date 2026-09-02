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

export CGO_ENABLED=0
go build -mod=vendor -ldflags "-s -w -X main.Version=$version" -o glow
install -Dm755 glow $PKG/usr/bin/glow

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/glow.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Markdown Reader
GenericName=Document Viewer
Comment=Read Markdown, rendered
Exec=glow %F
Icon=x-office-document
Terminal=true
Categories=Office;Viewer;Utility;
Keywords=markdown;md;readme;render;glow;
EOF
chmod 644 "$PKG/usr/share/applications/glow.desktop"
