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

# THE PROJECT'S OWN DEBUG RIG ALREADY DEPENDS ON SERIAL ACCESS IT COULD NOT
# PROVIDE. Every udev rule this tree ships for FTDI, CP210x, CH341 and CDC-ACM
# grants the `dialout` group a device with nothing on the host able to open it;
# this is the program those rules exist for.
#
# There is no configure. UUCP_LOCK_DIR must match what everything else on the
# machine uses or two programs will each believe they hold the port.
make CC="$CC" CFLAGS="$CFLAGS -DUUCP_LOCK_DIR='\"/run/lock\"' -DHIGH_BAUD"
install -Dm755 picocom $PKG/usr/bin/picocom
install -Dm644 picocom.1 $PKG/usr/share/man/man1/picocom.1
