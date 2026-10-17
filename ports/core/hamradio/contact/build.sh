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

pip3 install --ignore-installed --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr .

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/contact.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Meshtastic (contact)
GenericName=Mesh Messaging
Comment=Chat, nodes and settings of a Meshtastic radio
Exec=contact
Icon=network-modem
Terminal=true
Categories=Network;Chat;
Keywords=meshtastic;lora;mesh;radio;chat;contact;
DESKTOP
chmod 644 "$PKG/usr/share/applications/contact.desktop"
