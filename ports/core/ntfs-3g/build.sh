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

# --with-fuse=internal: ntfs-3g's external path wants libfuse 2 and this tree
# ships fuse 3 only, so the bundled fuse-lite is the one that links.
# --enable-extras is what builds ntfsprogs — ntfsfix, ntfsresize, ntfsclone,
# ntfsundelete and the rest. Without it the package is the mount helper alone
# and the name is only half true.
./configure \
	--prefix=/usr \
	--libdir=/usr/lib \
	--sbindir=/usr/sbin \
	--mandir=/usr/share/man \
	--disable-static \
	--disable-ldconfig \
	--enable-extras \
	--enable-posix-acls \
	--enable-xattr-mappings \
	--with-fuse=internal
make
make DESTDIR=$PKG install

rm -f $PKG/usr/lib/*.la
