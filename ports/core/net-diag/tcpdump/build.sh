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

# --with-user=tcpdump makes a capture started as root drop to that account
# once the device is open; postinstall.sh creates it, and tcpdump refuses to
# run when it is missing. libcap-ng is what keeps -w working after the drop:
# CAP_DAC_OVERRIDE is held until the savefile is open.
./configure \
	--prefix=/usr \
	--mandir=/usr/share/man \
	--with-crypto \
	--with-cap-ng \
	--with-user=tcpdump \
	--without-smi
make
make DESTDIR=$PKG install
