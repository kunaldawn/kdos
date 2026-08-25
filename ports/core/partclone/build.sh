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

autoreconf -f -i

# ddrescue is for a DYING disk and reads every sector; this is for a healthy
# one and reads only the blocks the filesystem says are in use. Both ship,
# because they answer different questions.
#
# One --enable per filesystem, and each is gated on its library being present:
# without the flag the tool is simply not built, so a recipe that claims ext4
# and omits it produces a partclone that cannot read ext4.
./configure \
	--prefix=/usr \
	--enable-extfs \
	--enable-fat \
	--enable-exfat \
	--enable-ntfs \
	--enable-btrfs \
	--enable-xfs \
	--enable-f2fs
make
make DESTDIR=$PKG install
