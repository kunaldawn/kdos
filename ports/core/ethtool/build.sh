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
# 2020 answers with more detail — and configure quietly builds ioctl-only when
# libmnl is absent, so a build that succeeds can still report less than the
# driver knows. The dependency is load-bearing rather than optional.
./configure --prefix=/usr --sysconfdir=/etc --disable-static
make
make DESTDIR=$PKG install
