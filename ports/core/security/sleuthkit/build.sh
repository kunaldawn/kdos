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

export CFLAGS="$CFLAGS -Wno-error"
export CXXFLAGS="$CXXFLAGS -Wno-error"

patch -p1 -i "$PORT_SRC/lvm-header.patch"

autoreconf -f -i

export CPPFLAGS="$CPPFLAGS -Dlibewf_handle_read_random=libewf_handle_read_buffer_at_offset"

./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--disable-static \
	--disable-java \
	--disable-cppunit \
	--with-afflib \
	--with-libbfio \
	--with-libvhdi \
	--with-libvmdk \
	--with-libvslvm \
	--with-libewf \
	--with-zlib

for def in HAVE_LIBEWF HAVE_LIBAFFLIB HAVE_LIBBFIO HAVE_LIBVHDI HAVE_LIBVMDK \
	HAVE_LIBVSLVM HAVE_LIBZ HAVE_LIBSQLITE3; do
	grep -q "^#define $def 1" tsk/tsk_config.h || {
		echo "sleuthkit: $def not set by configure" >&2
		exit 1
	}
done

make LDFLAGS="$LDFLAGS" sigfind_LDFLAGS=
make DESTDIR=$PKG LDFLAGS="$LDFLAGS" sigfind_LDFLAGS= install
find "$PKG" -type f | while read -r f; do
	if readelf -d "$f" 2>/dev/null | grep -q /var/cache; then
		echo "sleuthkit: $f has an RPATH into the build tree" >&2
		exit 1
	fi
done
