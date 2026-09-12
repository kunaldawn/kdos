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

# THE FAR SIDE IS A SERVER AND THE NEAR SIDE IS ~/Mail. mbsync moves messages
# and does nothing else: it builds no index and sends nothing, so it is one
# third of a mail system whose other two thirds are notmuch and msmtp. All
# three are pointed at the same Maildir root by the templates in /etc/skel,
# and a machine where they disagree has mail in a place nothing reads.
#
# PERL IS A BUILD DEPENDENCY, NOT A CONVENIENCE. configure aborts with
# "perl not found" and then demands v5.14, and src/Makefile.am generates
# drv_proxy.inc with drv_proxy_gen.pl — the .inc is not in the tarball, so a
# tree without perl fails at the first object rather than at configure.
#
# --without-sasl IS THE XOAUTH2 CEILING, AND IT IS mbsync'S ALONE. isync reaches
# XOAUTH2 and OAUTHBEARER only through cyrus-sasl, which this tree does not
# build, so an account whose provider has withdrawn application passwords fails
# here at run time on a mechanism the library does not have — and a token
# minter does not lift it, because there is nothing to present one to.
#
# THE OTHER TWO CAN. aerc speaks `imaps+oauthbearer://` in its own Go code and
# msmtp reports `Authentication library: built-in` with `oauthbearer` and
# `xoauth2` among its methods, so such an account is readable in aerc and
# sendable through msmtp. What it cannot be is MIRRORED into a local Maildir.
#
# NO mdconvert. It needs Berkeley DB >= 4.1, which configure probes for by
# linking rather than by a flag, and this tree builds no libdb — so the program
# is not built and USE_DB stays undefined. That costs nothing: the Maildir
# driver's only use of it is the legacy uid map.
./configure \
	--prefix=/usr \
	--sysconfdir=/etc \
	--with-ssl \
	--with-zlib \
	--without-sasl
make
make DESTDIR=$PKG install
