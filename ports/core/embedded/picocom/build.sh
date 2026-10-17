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

patch -p1 -i "$PORT_SRC/pinfo-prototype.patch"

make CC="$CC" CFLAGS="$CFLAGS -DUUCP_LOCK_DIR='\"/run/lock\"' -DHIGH_BAUD -DNO_CUSTOM_BAUD"
install -Dm755 picocom $PKG/usr/bin/picocom
install -Dm644 picocom.1 $PKG/usr/share/man/man1/picocom.1
