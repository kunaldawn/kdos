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

./autogen.sh
# configure finds struct termios2, which serves any baud rate, in
# <asm/termbits.h>; its TCGETS2 and TCSETS2 requests are in <asm/ioctls.h>,
# which glibc's <sys/ioctl.h> includes and musl's does not.
./configure --prefix=/usr --libdir=/usr/lib --disable-static --disable-tests \
	CPPFLAGS="$CPPFLAGS -include asm/ioctls.h"
make
make DESTDIR=$PKG install
