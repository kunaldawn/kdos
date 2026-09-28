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


# The Reticulum client that needs no toolkit: pages, messages and file
# transfer in urwid. It reads ~/.nomadnetwork and starts its own Reticulum
# instance, or attaches to an rnsd already running for the user.
pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/nomadnet.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Nomad Network
GenericName=Mesh Messaging
Comment=Messages, pages and files over a Reticulum mesh
Exec=nomadnet
Icon=network-wireless
Terminal=true
Categories=Network;Chat;
Keywords=reticulum;lxmf;mesh;lora;radio;chat;nomadnet;
DESKTOP
chmod 644 "$PKG/usr/share/applications/nomadnet.desktop"
