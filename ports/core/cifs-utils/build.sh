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

# autoconf's AC_HEADER_STDBOOL tests `#ifndef bool`. In C23 `bool` is a
# KEYWORD rather than a macro, so the probe concludes stdbool.h does not
# conform and configure stops with "necessary header(s) not found" — a message
# that names the header rather than the language edition. This code is C11 and
# says so.
export CFLAGS="$CFLAGS -std=gnu17"

# cifs.upcall and cifs.idmap want krb5, which is not a port; the ACL pair and
# the pam module want libraries this host does not carry either. mount.cifs is
# the deliverable and needs none of them.
# ROOTSBINDIR because the default puts mount.cifs in /sbin, and /sbin is a
# symlink to /usr/sbin here — installing through it would put the file outside
# the package's own manifest.
./configure \
	--prefix=/usr \
	--sbindir=/usr/sbin \
	--disable-cifsupcall \
	--disable-cifsidmap \
	--disable-cifsacl \
	--disable-pam \
	--disable-systemd \
	--disable-pythontools
make
make install DESTDIR=$PKG ROOTSBINDIR=/usr/sbin
