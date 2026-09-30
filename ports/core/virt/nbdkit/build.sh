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

# Every "check" option is named, so what is built does not follow what the
# build root happens to hold. The language plugins (Perl, Python, OCaml,
# Rust, Tcl, Lua, Go) and the Python-based S3 plugin are off: each would
# embed an interpreter for plugins nobody here writes. The libvirt plugin is
# off because libvirt's own nbdkit backend runs this server, and linking the
# two both ways makes a dependency cycle. The NFS plugin links libnfs. The
# torrent plugin is off: nothing here serves a disk out of a BitTorrent
# swarm, and it would put libtorrent-rasterbar under libvirt. libblkio,
# libguestfs, OpenCL and SELinux are not ported; VDDK loads VMware's
# proprietary library. USDT probes need systemtap's sys/sdt.h, which is not
# ported. linuxdisk runs mke2fs -d from e2fsprogs.
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
	--without-iso \
	--without-libblkio \
	--without-libguestfs \
	--without-selinux \
	--without-bash-completions \
	--disable-static
make
make DESTDIR=$PKG install
