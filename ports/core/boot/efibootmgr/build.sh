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

# "WINDOWS UPDATE WIPED MY ENTRY" IS THE CASE THIS EXISTS FOR, and it is one
# kinstall cannot cover: the installer writes an entry once, on a machine it is
# installing to, and every later firmware that reorders or drops it needs a
# tool on the running system. A KDOS install that stops booting after a dual
# boot has no other repair path that does not involve a firmware menu.
#
# It needs efivarfs mounted at /sys/firmware/efi/efivars; on a BIOS machine
# every command answers that there are no variables, which is the honest
# result and not an error.
export CFLAGS="$CFLAGS -Wno-error -Wno-address -Wno-stringop-overflow"
make EFIDIR=kdos libdir=/usr/lib bindir=/usr/sbin mandir=/usr/share/man
make EFIDIR=kdos libdir=/usr/lib bindir=/usr/sbin mandir=/usr/share/man DESTDIR=$PKG install
