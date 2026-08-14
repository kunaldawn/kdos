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

# --disable-backtrace: musl has no execinfo, and btrfs-progs' own configure
# does not probe for it — leaving this on is an undefined-reference link error
# at the very end of a long build.
# --disable-python and --disable-documentation keep sphinx off the build host.
./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--bindir=/usr/bin \
	--disable-static \
	--disable-backtrace \
	--disable-python \
	--disable-documentation \
	--disable-zoned

make
make DESTDIR=$PKG install
