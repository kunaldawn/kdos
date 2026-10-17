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

subs="libc dnsmasq named pdnsd pdns_recursor unbound"
./configure --prefix=/usr --sysconfdir=/etc --sbindir=/usr/sbin \
	--libexecdir=/usr/lib/resolvconf --rundir=/run --mandir=/usr/share/man
make SUBSCRIBERS="$subs"
make SUBSCRIBERS="$subs" DESTDIR=$PKG install
