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
# THE PATCH IS THE RULE-7 CASE AGAIN: no flag fixes it. picocom.c calls
# pinfo() at line 347 and defines it at 641 with no prototype in between —
# and that caller is inside the UUCP_LOCK_DIR block, which upstream's own
# Makefile never compiles because it builds with USE_FLOCK. A compiler that
# rejects implicit declarations then reports BOTH the call and the definition,
# the second as `conflicting types`, which no -Wno- can answer: an
# unprototyped `int pinfo()` is genuinely incompatible with a varargs
# definition.
patch -p1 -i "$PORT_SRC/pinfo-prototype.patch"

# There is no configure. UUCP_LOCK_DIR must match what everything else on the
# machine uses or two programs will each believe they hold the port.
#
# NO_CUSTOM_BAUD, AND IT COSTS ARBITRARY BAUD RATES. picocom's custom-baud path
# is Linux's `struct termios2` and the TCSETS2 ioctls, which live in
# <asm/termbits.h> — musl does not expose them and names its own termios
# fields `__c_ispeed`, so termbits2.h fails on `'TCSETS2' undeclared` and
# termios2.c on a member that "has no member named c_ispeed". What is lost is
# a rate that is not one of the standard constants; HIGH_BAUD keeps everything
# up to 4000000, which covers every adapter this tree has a udev rule for.
make CC="$CC" CFLAGS="$CFLAGS -DUUCP_LOCK_DIR='\"/run/lock\"' -DHIGH_BAUD -DNO_CUSTOM_BAUD"
install -Dm755 picocom $PKG/usr/bin/picocom
install -Dm644 picocom.1 $PKG/usr/share/man/man1/picocom.1
