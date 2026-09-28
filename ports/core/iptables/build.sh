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

# --disable-connlabel and --disable-libnfnetlink: libnetfilter_conntrack and
# libnfnetlink are not ported, and configure fails on the first when it is on.
# Both the nft and the legacy multi-call binaries are built; install points
# iptables and ip6tables at the legacy one, so they are repointed below at
# xtables-nft-multi, the one that writes nftables rules and so shares the
# kernel's ruleset with nft and libvirt's network driver. ebtables and
# arptables are nft-only already.
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
