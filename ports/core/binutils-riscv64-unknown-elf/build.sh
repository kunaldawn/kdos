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
# warnings upstream has not caught up with, and none of them is about riscv64-unknown-elf.
mkdir -p build && cd build
../configure \
	--target=riscv64-unknown-elf \
	--prefix=/usr \
	--with-sysroot=/usr/riscv64-unknown-elf \
	--disable-nls \
	--disable-werror \
	--disable-gdb \
	--disable-sim \
	--enable-multilib \
	--with-system-zlib \
	--with-zstd \
	--with-xxhash \
	--without-debuginfod \
	--without-msgpack \
	--with-pkgversion="KDOS"
make
make DESTDIR=$PKG install
# The info manuals and bfd-plugins/libdep.so carry the host binutils' own
# names and would collide with it. The man1 pages carry the target prefix and
# stay, less the three for Windows tools this target does not build.
rm -rf "$PKG/usr/share/info" "$PKG/usr/share/locale" "$PKG/usr/lib/bfd-plugins"
rm -f "$PKG"/usr/share/man/man1/riscv64-unknown-elf-{dlltool,windmc,windres}.1
