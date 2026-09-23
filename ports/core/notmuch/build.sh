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

# MAIL AS A DATABASE, WHICH IS WHY IT IS HERE AND A CLIENT IS NOT. notmuch does
# not fetch, send or store mail: it indexes a Maildir that something else put
# there and answers queries against it. That split is what lets aerc, mutt and
# a shell script all see the same tags.
#
# IT RIDES THE XAPIAN THE KIWIX AND recoll STACKS ALREADY NEED — the same
# library that gives a ZIM full-text search and the filesystem one gives mail
# its index, so this costs a binary rather than a search engine.
#
# Its configure is hand-written, not autoconf: --without-emacs and
# --with-bash-completion are its own spellings, and it has no --disable-*
# family. python3 is a requirement, not an option: configure stops without it
# and notmuch-git is a python script. The cffi bindings are staged inside the
# source tree when cffi is importable, and nothing installs them.
#
# Every --with-* below is a request that configure quietly drops when its tool
# is absent, so `depends` is what holds each one: sphinx-build for the manual
# pages (and makeinfo for the info pages built beside them), doxygen for
# notmuch(3), bash-completion's pkg-config file for the completion. configure
# also runs gpg against gmime's gpgme to prove session-key support and stops
# if it cannot. s-expression queries follow sfsexp, which is not a port and
# which configure has no switch for.
./configure --prefix=/usr --libdir=/usr/lib \
	--with-docs \
	--with-api-docs \
	--with-bash-completion \
	--with-zsh-completion \
	--with-retry-lock \
	--without-emacs \
	--without-desktop \
	--without-ruby
make
make DESTDIR=$PKG install
