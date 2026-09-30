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
go build -mod=vendor -ldflags "-s -w -X main.version=$version" -o lazydocker
install -Dm755 lazydocker $PKG/usr/bin/lazydocker

# THE ENTRY RUNS BEHIND kdos-podman-api. lazydocker speaks the Docker API,
# which on this image is `podman system service`; the wrapper starts it when
# nothing answers on this user's socket, names it in $DOCKER_HOST, and the
# service exits by itself once no client has used it for its idle timeout.
# lazydocker typed at a prompt reaches the same socket only while that service
# is running.
install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/lazydocker.desktop" <<'EOF2'
[Desktop Entry]
Type=Application
Name=Containers (lazydocker)
GenericName=Container Manager
Comment=Containers, images, volumes and logs on one screen
Exec=kdos-podman-api lazydocker
Icon=network-server
Terminal=true
Categories=System;
Keywords=docker;podman;container;image;logs;lazydocker;
EOF2
chmod 644 "$PKG/usr/share/applications/lazydocker.desktop"
