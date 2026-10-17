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

./configure \
	--prefix=/usr \
	--sysconfdir=/etc \
	--libdir=/usr/lib \
	--disable-valgrind \
	--disable-probes \
	--disable-perl \
	--disable-python \
	--disable-ocaml \
	--disable-rust \
	--disable-tcl \
	--disable-lua \
	--disable-golang \
	--disable-torrent \
	--disable-vram \
	--disable-vddk \
	--disable-libguestfs-tests \
	--enable-linuxdisk \
	--with-gnutls \
	--with-iconv \
	--with-curl \
	--with-ssh \
	--with-zlib \
	--without-zlib-ng \
	--with-bzip2 \
	--with-liblzma \
	--with-libzstd \
	--with-libnbd \
	--with-ext2 \
	--with-manpages \
	--without-libvirt \
	--with-nfs \
	--with-iso \
	--without-libblkio \
	--without-libguestfs \
	--without-selinux \
	--with-bash-completions \
	--disable-static
make
make DESTDIR=$PKG install
