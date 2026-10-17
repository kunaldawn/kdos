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

LIBS="$PORT_SRC/../../libs"

gcc $CFLAGS -O2 -std=gnu11 -D_GNU_SOURCE -DKDOS_HAVE_LIBPNG -Wall -Wextra \
	-I"$LIBS/libkbase" -I"$LIBS/libkcolor" -I"$LIBS/libkpkg" \
	-I"$LIBS/libkxdg" -I"$LIBS/libkproc" -I"$LIBS/libksig" \
	-I"$LIBS/libkpack" -I"$PORT_SRC" \
	-o kdos-tools \
	"$PORT_SRC"/*.c \
	"$LIBS"/libkbase/*.c "$LIBS"/libkcolor/*.c "$LIBS"/libkpkg/*.c \
	"$LIBS"/libkxdg/*.c "$LIBS"/libkproc/*.c \
	"$LIBS"/libksig/*.c "$LIBS"/libksig/monocypher/*.c \
	"$LIBS"/libkpack/*.c -lpng $LDFLAGS

install -Dm755 kdos-tools "$PKG/usr/sbin/ksvc"

ln -s ksvc "$PKG/usr/sbin/service"
install -d "$PKG/usr/local/sbin"
ln -s /usr/sbin/ksvc "$PKG/usr/local/sbin/kdos-getty"

install -d "$PKG/usr/local/bin"
for t in kdos kdos-banner kdos-shot kdos-share kdos-fetch-app \
	 kdos-fetch-static kdos-sfx kdos-mpctl; do
	ln -s /usr/sbin/ksvc "$PKG/usr/local/bin/$t"
done

install -d "$PKG/usr/bin"
ln -s /usr/sbin/ksvc "$PKG/usr/bin/kdos-bootctl"

install -d "$PKG/usr/share/kdos/reasons"
install -m644 "$PORT_SRC"/reasons/*.txt "$PKG/usr/share/kdos/reasons/"

install -Dm644 "$PORT_SRC/secdb/secdb.txt" "$PKG/usr/share/kdos/secdb.txt"
install -Dm644 "$PORT_SRC/secdb/bundled.txt" "$PKG/usr/share/kdos/bundled.txt"

install -d "$PKG/usr/share/applications"
cat > "$PKG/usr/share/applications/kdos-help.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=KDOS Help
GenericName=Keybinds and commands
Comment=KDOS cheat sheet — commands and keybinds
Exec=kdos help --pager
Icon=help-contents
Terminal=true
Categories=System;Documentation;
Keywords=help;keys;keybinds;cheatsheet;kdos;
EOF
chmod 644 "$PKG/usr/share/applications/kdos-help.desktop"
