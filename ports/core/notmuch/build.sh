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
# --without-bash-completion are its own spellings, and it has no --disable-*
# family. The python bindings are a separate directory nothing here builds.
./configure --prefix=/usr --libdir=/usr/lib \
	--without-emacs \
	--without-desktop \
	--without-api-docs \
	--without-ruby
make
make DESTDIR=$PKG install
