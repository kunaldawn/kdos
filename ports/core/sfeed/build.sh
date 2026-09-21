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

# MANPREFIX IS NOT UNDER PREFIX HERE. Upstream's default is ${PREFIX}/man,
# which on this image is /usr/man — a directory `man` does not search and
# nothing else on the filesystem uses.
#
# -lncurses AND NOT -lcurses. The ncurses port installs libcurses.so as a
# symlink, so the default links today; naming the real library is what keeps
# this building if that compatibility link ever goes.
export CFLAGS="$CFLAGS -O2"
make PREFIX=/usr MANPREFIX=/usr/share/man \
	SFEED_CURSES_LDFLAGS="$LDFLAGS -lncurses"
make DESTDIR=$PKG PREFIX=/usr MANPREFIX=/usr/share/man \
	DOCPREFIX=/usr/share/doc/sfeed install

# THE READER IS AN APPLICATION AND UPSTREAM SHIPS NO ENTRY. `sfeed_curses`
# reads the files `sfeed_update` writes, so the entry names the reader and not
# the fetcher: a menu row that started a download and showed nothing would look
# like a program that did not start.
install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/sfeed.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Feeds (sfeed)
GenericName=Feed Reader
Comment=Read the feeds sfeed_update has fetched
Exec=sfeed_curses
Icon=mail-message
Terminal=true
Categories=Network;News;
Keywords=rss;atom;feed;news;sfeed;
EOF
chmod 644 "$PKG/usr/share/applications/sfeed.desktop"
