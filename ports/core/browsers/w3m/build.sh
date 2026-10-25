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

./configure \
	--prefix=/usr \
	--libexecdir=/usr/lib \
	--sysconfdir=/etc \
	--enable-image=fb \
	--with-imagelib=gtk2 \
	--disable-xface \
	--with-browser=xdg-open \
	--disable-w3mmailer \
	--disable-nls \
	--with-termlib=ncurses \
	--with-ssl

make
make DESTDIR=$PKG install

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/w3m.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Web Browser (w3m)
GenericName=Web Browser
Comment=Browse the web on the grid
Exec=w3m -v %u
Icon=gtk-network
Terminal=true
Categories=Network;WebBrowser;
Keywords=web;browser;http;html;w3m;
EOF
chmod 644 "$PKG/usr/share/applications/w3m.desktop"
