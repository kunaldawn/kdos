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

# EVERY DOCUMENTED APPBOX FAILURE IS PODMAN STATE. A box stuck in `stopping`, a
# half-built container init, a storage driver that does not match what the
# store was written with — each is diagnosed by looking at containers, images
# and volumes, and until now that meant remembering the podman subcommand for
# it while the desktop was already misbehaving. It talks to the rootless socket
# the same way kdos-appbox does, so it sees exactly the boxes the desktop
# launched.
#
# containers_image_openpgp swaps the cgo gpgme binding for the pure-Go one, and
# excluding the btrfs graph driver drops a cgo header dependency — both are
# what keep CGO_ENABLED=0 achievable.
export CGO_ENABLED=0
go build -mod=vendor -tags "containers_image_openpgp exclude_graphdriver_btrfs" \
	-ldflags "-s -w" -o podman-tui
install -Dm755 podman-tui $PKG/usr/bin/podman-tui

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/podman-tui.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Containers
GenericName=Container Manager
Comment=Images, containers, pods and volumes
Exec=podman-tui
Icon=system-software-install
Terminal=true
Categories=System;
Keywords=container;podman;image;pod;box;
EOF
chmod 644 "$PKG/usr/share/applications/podman-tui.desktop"
