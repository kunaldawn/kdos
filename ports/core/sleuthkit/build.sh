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

# -Wno-error, and the same reason as stlink: upstream compiles with -Werror,
# which is a promise about the libc it builds against. musl's <sys/fcntl.h> is
# a compat shim whose whole body is a `#warning` redirecting to <fcntl.h>, and
# -Werror=cpp makes that fatal in a header sleuthkit does not include directly.
# Every warning is still printed; upstream just stops deciding which of them
# ends this build.
export CFLAGS="$CFLAGS -Wno-error"
export CXXFLAGS="$CXXFLAGS -Wno-error"

autoreconf -f -i

# IT READS DAMAGED FILESYSTEMS mount(8) REFUSES, which is the property that
# earns it a place beside testdisk rather than overlapping it: testdisk repairs
# a partition table so the kernel will mount the thing, and this one never asks
# the kernel at all — `fls` and `icat` walk the metadata directly out of an
# image, so a filesystem too damaged to mount still gives up its files.
#
# --disable-java because Autopsy is a Java application and is not shipped; the
# bindings would build a jar nothing on this machine can run.
#
# E01 IS READ THROUGH THE libewf PORT; AFF IS NOT. libewf is the container
# format most acquisitions arrive in, so an image from somebody else's tool
# opens with `fls` directly. afflib, libvhdi, libvmdk and libvslvm are not
# ports: a raw image, a split raw image, a live device and an E01 all work, and
# an AFF, VHD(X), VMDK or an LVM volume inside an image does not.
#
# The libewf port exports libewf_handle_read_buffer_at_offset and no
# libewf_handle_read_random, which is the name sleuthkit 4.15.0 calls. Both take
# the same arguments; the define maps the one call at compile time, and without
# it ewf.cpp fails on an undeclared function.
export CPPFLAGS="$CPPFLAGS -Dlibewf_handle_read_random=libewf_handle_read_buffer_at_offset"

./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--disable-static \
	--disable-java \
	--disable-cppunit \
	--without-afflib \
	--without-libbfio \
	--without-libvhdi \
	--without-libvmdk \
	--without-libvslvm \
	--with-libewf \
	--with-zlib

# --with-<lib> still falls back silently when the library does not link, and
# sqlite has no switch at all: without a system one it compiles the bundled
# copy. The defines below are what each one actually reached the build as, so
# a missing dependency stops here instead of shipping a narrower tool.
for def in HAVE_LIBEWF HAVE_LIBZ HAVE_LIBSQLITE3; do
	grep -q "^#define $def 1" tsk/tsk_config.h || {
		echo "sleuthkit: $def not set by configure" >&2
		exit 1
	}
done

make
make DESTDIR=$PKG install
