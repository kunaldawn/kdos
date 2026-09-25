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

# IT RENDERS TABLES, WHICH IS THE WHOLE REASON IT IS HERE AND NOT lynx. Most of
# the offline corpus is HTML — ZIM articles, saved documentation, a package's
# own docs — and technical HTML is full of tables that lynx flattens into
# unreadable runs of text. w3m lays them out on the character grid, which is
# the same grid everything else on this desktop draws into.
#
# INLINE PICTURES ARE BUILT AND OFF. display_image defaults to off, and no
# /etc/w3m/config ships to turn it on: once any inline_img_protocol is set,
# every interactive start asks the terminal for its cell size. A terminal that
# neither fills in the pixel size of its window nor answers `CSI 14 t` — the
# Linux console, kdos-term — holds every start for two seconds, and the console
# is then sent sixel it cannot draw. Under foot, `inline_img_protocol 2` and
# `display_image 1` in ~/.w3m/config have w3m pipe each image through
# libsixel's img2sixel. --enable-image=fb builds only the framebuffer
# w3mimgdisplay, never the X one, and w3m still needs it to measure an image
# whose header it cannot parse. --with-imagelib=gtk2 names nothing from GTK:
# that backend's framebuffer branch links gdk-pixbuf-2.0 alone, where the
# gdk-pixbuf one wants the long-gone gdk-pixbuf-config. X-Face defaults to the
# image setting and needs uncompface, which is not a port.
#
# --with-browser=xdg-open, a bare name that w3m hands to the shell: PATH finds
# kdos-appbox's resolver in /usr/local/bin before xdg-utils' script, so an
# external-browser request goes where every other link on the desktop goes.
# The compiled-in default is /usr/bin/firefox.
make
make DESTDIR=$PKG install

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/w3m.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Web Browser
GenericName=Web Browser
Comment=Browse the web on the grid
Exec=w3m %u
Icon=gtk-network
Terminal=true
Categories=Network;WebBrowser;
Keywords=web;browser;http;html;w3m;
EOF
chmod 644 "$PKG/usr/share/applications/w3m.desktop"
