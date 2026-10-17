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
	--enable-nftables \
	--disable-connlabel \
	--disable-libnfnetlink \
	--disable-static
make
make DESTDIR=$PKG install

for t in iptables iptables-save iptables-restore \
	ip6tables ip6tables-save ip6tables-restore; do
	ln -sf xtables-nft-multi "$PKG/usr/sbin/$t"
done
