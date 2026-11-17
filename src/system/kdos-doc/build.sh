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

python3 "$PORT_SRC/mkbook.py" "$PORT_SRC" "$PKG" "$PORT_SRC/../.."

gcc $CFLAGS -O2 -std=gnu11 -Wall -Wextra -o kdos-doc "$PORT_SRC/kdos-doc.c" $LDFLAGS
install -Dm755 kdos-doc "$PKG/usr/bin/kdos-doc"

install -Dm644 -t "$PKG/usr/share/khelpcenter/plugins" "$PORT_SRC"/khelpcenter/kdos.desktop
install -Dm644 -t "$PKG/usr/share/applications" "$PORT_SRC"/applications/kdos-library-kdos-book.desktop

install -d "$PKG/usr/share/doc/HTML/en"
ln -s ../../kdos "$PKG/usr/share/doc/HTML/en/kdos"
