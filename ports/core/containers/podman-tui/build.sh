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
go build -mod=vendor -tags "remote containers_image_openpgp exclude_graphdriver_btrfs" \
	-ldflags "-s -w" -o podman-tui
install -Dm755 podman-tui $PKG/usr/bin/podman-tui

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/podman-tui.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Containers
GenericName=Container Manager
Comment=Images, containers, pods and volumes
Exec=kdos-podman-api podman-tui
Icon=network-server
Terminal=true
Categories=System;
Keywords=container;podman;image;pod;box;
EOF
chmod 644 "$PKG/usr/share/applications/podman-tui.desktop"
