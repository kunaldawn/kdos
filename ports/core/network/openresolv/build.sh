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
restart='"/usr/sbin/service status \\$$1 >/dev/null 2>\&1 \&\& /usr/sbin/service restart \\$$1"'
./configure --prefix=/usr --sysconfdir=/etc --sbindir=/usr/sbin \
	--libexecdir=/usr/lib/resolvconf --rundir=/run --mandir=/usr/share/man
make SUBSCRIBERS="$subs" RESTARTCMD="$restart"
make SUBSCRIBERS="$subs" RESTARTCMD="$restart" DESTDIR=$PKG install
