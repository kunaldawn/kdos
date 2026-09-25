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

# pppd is started by NetworkManager, which is root, so it is installed without
# the setuid bit a dial-up-by-hand setup would want. PAM is off: pppd's own
# login authentication is for a machine that answers calls, not one that makes
# them.
./configure --prefix=/usr --sysconfdir=/etc --localstatedir=/var --runstatedir=/run \
	--disable-static \
	--disable-systemd \
	--without-pam \
	--with-pcap=yes \
	--with-runtime-dir=/run/pppd \
	--with-logfile-dir=/var/log/ppp
make
make DESTDIR=$PKG install
