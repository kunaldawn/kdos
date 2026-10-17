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

go build -mod=vendor -trimpath -ldflags "-s -w" -o pat-bin .
install -Dm755 pat-bin "$PKG/usr/bin/pat"
install -Dm644 -t "$PKG/usr/share/man/man1" man/pat.1 man/pat-configure.1

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/pat.desktop" <<'ENTRY'
[Desktop Entry]
Type=Application
Name=Pat
GenericName=Winlink Email Client
Comment=Winlink email over radio through ARDOP, AX.25 or VARA; serves its web interface on localhost:8083
Exec=pat http --addr localhost:8083
Icon=network-wireless
Terminal=true
Categories=Network;HamRadio;Email;
Keywords=ham;radio;winlink;email;ardop;ax25;packet;emcomm;
ENTRY
chmod 644 "$PKG/usr/share/applications/pat.desktop"
