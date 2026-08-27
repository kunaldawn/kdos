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

./configure \
	--prefix=/usr \
	--libexecdir=/usr/lib \
	--sysconfdir=/etc \
	--enable-image=no \
	--disable-w3mmailer \
	--with-termlib=ncurses \
	--with-ssl

# IT RENDERS TABLES, WHICH IS THE WHOLE REASON IT IS HERE AND NOT lynx. Most of
# the offline corpus is HTML — ZIM articles, saved documentation, a package's
# own docs — and technical HTML is full of tables that lynx flattens into
# unreadable runs of text. w3m lays them out on the character grid, which is
# the same grid everything else on this desktop draws into.
#
# --enable-image=no: the inline image support wants X or a framebuffer helper.
# Under foot, `w3m` plus a sixel viewer covers the case, and the X path is the
# hard rule.
make
make DESTDIR=$PKG install
