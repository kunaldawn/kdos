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
# add it to the Makefile's SRCS from the command line, mirroring Alpine's
# netcat-openbsd port.
patch -p1 -i b64.patch

# -T va names the RFC 5865 Voice-Admit code point, IPTOS_DSCP_VA, which
# glibc's <netinet/ip.h> defines and musl's does not; its value is 0xb0.
make SRCS="netcat.c atomicio.c socks.c base64.c" \
	CFLAGS="$CFLAGS -DIPTOS_DSCP_VA=0xb0 -DDEBIAN_VERSION=\"\\\"$version-$debrev\\\"\"" LDFLAGS="$LDFLAGS"
install -Dm755 nc $PKG/usr/bin/nc
install -Dm644 nc.1 $PKG/usr/share/man/man1/nc.1
