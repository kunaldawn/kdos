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
#
# THE ENTRY RUNS sfeed-read, NOT sfeed_curses. With no file arguments
# sfeed_curses reads a feed from stdin, which from a launcher is the terminal:
# a blank window waiting for Ctrl-D. The wrapper names every file under
# sfeed_update's default ~/.sfeed/feeds; with none there yet it says how to
# fetch some instead, because a glob that matched nothing would reach
# sfeed_curses as a literal pattern it then fails to open.
install -d "$PKG/usr/bin"
cat > "$PKG/usr/bin/sfeed-read" <<'KDOS_SH'
#!/bin/sh
dir="$HOME/.sfeed/feeds"
set -- "$dir"/*
if [ -e "$1" ]; then
	exec sfeed_curses "$@"
fi
printf 'No feeds in %s yet.\n' "$dir"
printf 'List them in ~/.sfeed/sfeedrc (see sfeedrc(5)), then run sfeed_update.\n'
printf 'Press Enter to close. '
read -r _
KDOS_SH
chmod 755 "$PKG/usr/bin/sfeed-read"

# THE YANK KEY COPIES THROUGH wl-copy. sfeed_curses pipes the URL to
# $SFEED_YANKER, whose default is `xclip -r`, and there is no X clipboard here.
# A login shell reads this, and the desktop inherits it; a value somebody set
# first is kept.
install -Dm644 /dev/stdin "$PKG/etc/profile.d/50-sfeed.sh" <<'KDOS_SH'
export SFEED_YANKER="${SFEED_YANKER:-wl-copy -n}"
KDOS_SH

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/sfeed.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=Feeds (sfeed)
GenericName=Feed Reader
Comment=Read the feeds sfeed_update has fetched
Exec=sfeed-read
Icon=mail-message
Terminal=true
Categories=Network;News;
Keywords=rss;atom;feed;news;sfeed;
EOF
chmod 644 "$PKG/usr/share/applications/sfeed.desktop"
