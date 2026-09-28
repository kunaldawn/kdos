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

# cgo with the libax25 tag links the libax25 port, which is what gives Pat
# the kernel AX.25 transport for packet radio; without it Pat has only its
# ARDOP, VARA and telnet transports. The web interface is embedded from
# web/dist as upstream publishes it.
export CGO_ENABLED=1
go build -mod=vendor -trimpath -tags libax25 -ldflags "-s -w" -o pat-bin .
install -Dm755 pat-bin "$PKG/usr/bin/pat"
install -Dm644 -t "$PKG/usr/share/man/man1" man/pat.1 man/pat-configure.1

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/pat.desktop" <<'ENTRY'
[Desktop Entry]
Type=Application
Name=Pat
GenericName=Winlink Email Client
Comment=Winlink email over radio through ARDOP, AX.25 or VARA; serves its web interface on localhost:8080
Exec=pat http
Icon=network-wireless
Terminal=true
Categories=Network;HamRadio;Email;
Keywords=ham;radio;winlink;email;ardop;ax25;packet;emcomm;
ENTRY
chmod 644 "$PKG/usr/share/applications/pat.desktop"
