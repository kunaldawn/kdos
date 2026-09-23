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
# --disable-python and --disable-documentation keep sphinx off the build host;
# the release tarball carries the manual pages prebuilt, installed as-is.
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

install -Dm644 Documentation/*.2 -t "$PKG/usr/share/man/man2"
install -Dm644 Documentation/*.5 -t "$PKG/usr/share/man/man5"
install -Dm644 Documentation/*.8 -t "$PKG/usr/share/man/man8"
