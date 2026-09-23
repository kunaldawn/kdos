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

# A CROSS BINUTILS AND NOTHING MORE. --with-sysroot names where the target's
# own headers and libraries will live so the linker searches there rather than
# in the host's /usr/lib, which is musl for x86_64 and would link silently
# wrong. --disable-werror because a binutils built by a newer GCC trips
# warnings upstream has not caught up with, and none of them is about arm-none-eabi.
mkdir -p build && cd build
../configure \
	--target=arm-none-eabi \
	--prefix=/usr \
	--with-sysroot=/usr/arm-none-eabi \
	--disable-nls \
	--disable-werror \
	--disable-gdb \
	--disable-sim \
	--enable-multilib \
	--with-pkgversion="KDOS"
make
make DESTDIR=$PKG install
# The info manuals and bfd-plugins/libdep.so carry the host binutils' own
# names and would collide with it. The man1 pages carry the target prefix and
# stay, less the three for Windows tools this target does not build.
rm -rf "$PKG/usr/share/info" "$PKG/usr/share/locale" "$PKG/usr/lib/bfd-plugins"
rm -f "$PKG"/usr/share/man/man1/arm-none-eabi-{dlltool,windmc,windres}.1
