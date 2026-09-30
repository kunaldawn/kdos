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

# SUBSCRIBERS is the list of resolvers resolvconf knows how to feed. Upstream's
# default adds two that hand the servers to systemd-resolved, which is not on
# this system, so the list is named without them.
subs="libc dnsmasq named pdnsd pdns_recursor unbound"
./configure --prefix=/usr --sysconfdir=/etc --sbindir=/usr/sbin \
	--libexecdir=/usr/lib/resolvconf --rundir=/run --mandir=/usr/share/man
make SUBSCRIBERS="$subs"
make SUBSCRIBERS="$subs" DESTDIR=$PKG install
