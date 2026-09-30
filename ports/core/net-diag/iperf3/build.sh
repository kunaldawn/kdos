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

# --with-openssl makes a missing libssl a configure failure instead of an
# iperf3 with no --username/--rsa-public-key. SCTP needs lksctp-tools, which
# is not a port, so it is off rather than left to a header probe.
./configure --prefix=/usr --mandir=/usr/share/man --disable-static --without-ldconfig \
	--with-openssl=/usr \
	--without-sctp
make
# --without-ldconfig leaves LDCONFIG empty, and the install hook's
# `if test -n "$(LDCONFIG)"; then $(LDCONFIG) || true; fi` is then a shell
# syntax error that fails the install. LDCONFIG=: makes the hook a no-op.
make DESTDIR=$PKG LDCONFIG=: install
