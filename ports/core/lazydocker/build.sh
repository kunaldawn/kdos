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

# THE SOCKET IS NOT STARTED HERE AND THE ENTRY DOES NOT WRAP IT. lazydocker
# speaks the Docker API; on this image that is `podman system service`, which
# has to be running and $DOCKER_HOST has to name it. An Exec that started a
# daemon behind a menu row would be a row that leaves a service running after
# the window closes, and one that exported a variable would need a shell.
# Turning the service on belongs to the settings surface.
install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/lazydocker.desktop" <<'EOF2'
[Desktop Entry]
Type=Application
Name=Containers
GenericName=Container Manager
Comment=Containers, images, volumes and logs on one screen
Exec=lazydocker
Icon=applications-system
Terminal=true
Categories=System;
Keywords=docker;podman;container;image;logs;lazydocker;
EOF2
chmod 644 "$PKG/usr/share/applications/lazydocker.desktop"
