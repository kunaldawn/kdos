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

make -C src init halt shutdown runlevel killall5 VERSION=$version \
	CFLAGS="$CFLAGS -I/usr/include/utmps" LDLIBS="-lutmps -lskarnet"

install -Dm755 src/init src/halt src/shutdown src/runlevel src/killall5 -t "$PKG/sbin"
ln -s init "$PKG/sbin/telinit"
ln -s halt "$PKG/sbin/reboot"
ln -s halt "$PKG/sbin/poweroff"

install -Dm644 man/init.8 man/telinit.8 man/halt.8 man/reboot.8 man/poweroff.8 \
	man/shutdown.8 man/runlevel.8 man/killall5.8 -t "$PKG/usr/share/man/man8"
install -Dm644 man/inittab.5 man/initctl.5 -t "$PKG/usr/share/man/man5"
