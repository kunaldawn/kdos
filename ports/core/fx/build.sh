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
go build -mod=vendor -ldflags "-s -w" -o fx
install -Dm755 fx $PKG/usr/bin/fx

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/fx.desktop" <<'ENTRY'
[Desktop Entry]
Type=Application
Name=JSON Viewer
GenericName=Data Viewer
Comment=Browse, fold and query JSON and YAML
Exec=fx %f
Icon=text-x-generic
Terminal=true
Categories=Development;Utility;
Keywords=json;yaml;viewer;jq;query;fx;
ENTRY
chmod 644 "$PKG/usr/share/applications/fx.desktop"
