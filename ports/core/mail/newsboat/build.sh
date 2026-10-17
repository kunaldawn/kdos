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

export CARGO_NET_OFFLINE=true

export LDFLAGS="$LDFLAGS -lintl"

export CXXFLAGS="$CXXFLAGS -Wno-error=unused-function -g0"

make prefix=/usr newsboat podboat mo-files doc/newsboat.1 doc/podboat.1
make prefix=/usr DESTDIR=$PKG install-newsboat install-podboat install-mo \
	install-completions
install -Dm644 doc/newsboat.1 doc/podboat.1 -t "$PKG/usr/share/man/man1"

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/newsboat.desktop" <<'ENTRY'
[Desktop Entry]
Type=Application
Name=Feeds
GenericName=Feed Reader
Comment=Read RSS and Atom feeds
Exec=newsboat
Icon=mail-message
Terminal=true
Categories=Network;News;
Keywords=rss;atom;feed;news;reader;newsboat;
ENTRY
chmod 644 "$PKG/usr/share/applications/newsboat.desktop"
