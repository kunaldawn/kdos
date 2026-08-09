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

while read -r p; do
	case "$p" in ''|\#*) continue ;; esac
	patch -p1 -i "debian/patches/$p"
done < "debian/patches/series"

# musl libresolv lacks b64_ntop; ship a self-contained encoder and
# wire it into the Makefile, mirroring Alpine's netcat-openbsd port.
patch -p1 -i b64.patch
sed -i -e '/SRCS=/s;\(.*\);& base64.c;' Makefile

make CFLAGS="$CFLAGS -DDEBIAN_VERSION=\"\\\"$version-$debrev\\\"\"" LDFLAGS="$LDFLAGS"
install -Dm755 nc $PKG/usr/bin/nc
install -Dm644 nc.1 $PKG/usr/share/man/man1/nc.1
