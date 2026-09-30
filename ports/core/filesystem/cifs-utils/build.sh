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

# `cifs.upcall` IS WHAT MAKES sec=krb5 REACHABLE, and it is the whole reason
# krb5 is in `depends`. The kernel's cifs module cannot get a ticket itself: it
# raises a `cifs.spnego` key request, `request-key` runs this helper against
# the calling user's credential cache, and the helper hands back the SPNEGO
# blob. Without it a server that will accept only a ticket answers
# `Required key not available` and nothing on the machine says which key.
#
# `cifs.idmap` AND THE ACL PAIR STAY OFF, AND IT IS ONE LIBRARY THAT DECIDES
# BOTH. Each wants `wbclient.h`, which comes from winbind, and samba here is
# built `--without-winbind` — configure stops with "wbclient.h not found"
# rather than degrading. What they would give is SID-to-uid mapping, and this
# image does not need it: `kdos-mountd` mounts every share with an explicit
# `uid=`, `gid=`, `file_mode=` and `dir_mode=`, so ownership on the mount is
# the caller's by construction rather than by translation.
#
# `pam` is off because it is not the authentication stack on this image —
# `authfw=shadow` is.
#
# `cifscreds` stashes a share's password in the session keyring through
# keyutils, and `smbinfo` and `smb2-quota` are standard-library python3 scripts
# that read a mounted share's metadata and quotas. cifscreds is named on
# because, left at its default, configure drops it with only a warning when
# keyutils.h is missing; the scripts are installed without any check for python3, so
# `depends` is what makes their interpreter present.
#
# mount.cifs drops its capabilities through libcap-ng; with libcap-ng absent
# configure falls back to libcap and then to keeping them all, so it is named.
#
# ROOTSBINDIR because the default puts mount.cifs in /sbin, and /sbin is a
# symlink to /usr/sbin here — installing through it would put the file outside
# the package's own manifest.
./configure \
	--prefix=/usr \
	--sbindir=/usr/sbin \
	--enable-cifsupcall \
	--enable-cifscreds \
	--enable-smbinfo \
	--enable-pythontools \
	--with-libcap-ng=yes \
	--disable-cifsidmap \
	--disable-cifsacl \
	--disable-pam \
	--disable-systemd \
	--enable-man
make
make install DESTDIR=$PKG ROOTSBINDIR=/usr/sbin
