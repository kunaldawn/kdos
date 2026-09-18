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
# --with-sasl IS WHAT MAKES XOAUTH2 REACHABLE, AND IT IS TWO PORTS AND NOT ONE.
# isync calls sasl_client_* and takes whichever mechanism a plugin under
# /usr/lib/sasl2 provides — cyrus-sasl is the loader and ships no XOAUTH2 of its
# own, so cyrus-sasl-xoauth2 is the plugin that carries the mechanism. Both are
# `depends`: with the library and without the plugin, mbsync links, negotiates,
# and still fails on an account whose provider has withdrawn application
# passwords.
#
# THE TOKEN IS THE PASSWORD. The plugin asks for SASL_CB_PASS and wraps it as
# `auth=Bearer <token>`, which mbsync fills from the account's `PassCmd` — so
# `AuthMechs XOAUTH2` beside `PassCmd "pizauth show <account>"` is the whole of
# it, and the minter this tree already carries is what it presents to.
#
# OAUTHBEARER IS STILL OUT. The plugin implements the one mechanism; a server
# offering only OAUTHBEARER is read in aerc, which speaks
# `imaps+oauthbearer://` in its own Go code, and sent through msmtp, which
# reports `Authentication library: built-in` with both methods.
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
	--with-sasl
make
make DESTDIR=$PKG install
