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

# libmnl IS WHAT MAKES THE NEW INTERFACE REACHABLE. ethtool speaks two
# protocols — the legacy ioctl and the netlink one every driver written since
# 2020 answers with more detail. --enable-netlink makes configure fail without
# libmnl; --disable-netlink would build ioctl-only, and that build reports less
# than the driver knows. The dependency is load-bearing rather than optional.
./configure --prefix=/usr --sysconfdir=/etc --disable-static \
	--enable-netlink \
	--with-bash-completion-dir=/usr/share/bash-completion/completions
make
make DESTDIR=$PKG install
