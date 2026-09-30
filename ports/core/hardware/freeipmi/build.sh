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

# argp comes from argp-standalone, because musl has none. IPMI 2.0
# encryption is libgcrypt. The init scripts and systemd units upstream
# installs for bmc-watchdog, ipmidetectd and ipmiseld are off: nothing here
# starts a daemon.
#
# musl gives a new thread a 128 KiB stack, which the tools' worker threads
# overflow; the linker note raises the default to 2 MiB.
export LDFLAGS="$LDFLAGS -Wl,-z,stack-size=0x200000"
./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--sysconfdir=/etc \
	--localstatedir=/var \
	--mandir=/usr/share/man \
	--disable-static \
	--disable-init-scripts \
	--without-systemdsystemunitdir
grep -q '^#define HAVE_ARGP_H 1' config/config.h
grep -q '^#define WITH_ENCRYPTION 1' config/config.h
make
make DESTDIR=$PKG install
