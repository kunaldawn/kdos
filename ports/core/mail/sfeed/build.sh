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

export CFLAGS="$CFLAGS -O2"
make PREFIX=/usr MANPREFIX=/usr/share/man \
	SFEED_CURSES_LDFLAGS="$LDFLAGS -lncurses"
make DESTDIR=$PKG PREFIX=/usr MANPREFIX=/usr/share/man \
	DOCPREFIX=/usr/share/doc/sfeed install

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
