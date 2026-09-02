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
export CGO_ENABLED=1
# notmuch IS the reason this is here rather than mutt: aerc queries a notmuch
# database directly, so mail that is already indexed on the machine is
# searchable from the client with no second index. That binding is cgo, which
# is why CGO_ENABLED is on for this port and off for every other Go one.
make PREFIX=/usr GOFLAGS="-mod=vendor -tags=notmuch"
make PREFIX=/usr DESTDIR=$PKG install

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/aerc.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Mail
GenericName=Email Client
Comment=Read and send mail
Exec=aerc
Icon=mail-message
Terminal=true
Categories=Network;Email;
Keywords=mail;email;imap;smtp;aerc;
EOF
chmod 644 "$PKG/usr/share/applications/aerc.desktop"
