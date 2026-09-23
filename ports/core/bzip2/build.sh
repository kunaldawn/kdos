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

make CFLAGS="$CFLAGS"
make PREFIX=$PKG/usr install

install -d $PKG/usr/share
mv $PKG/usr/man $PKG/usr/share/man
for l in bzegrep:bzgrep bzfgrep:bzgrep bzless:bzmore bzcmp:bzdiff; do
	ln -sf ${l#*:} $PKG/usr/bin/${l%:*}
done
