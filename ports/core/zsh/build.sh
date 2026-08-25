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

# THE COMPLETIONS ARE THE POINT. bash is the system shell and stays so; what
# this brings is Completion/, which is embedded knowledge of the arguments of a
# thousand commands and is worth more offline than online, where a search would
# otherwise answer the question.
./configure \
	--prefix=/usr \
	--sysconfdir=/etc/zsh \
	--enable-etcdir=/etc/zsh \
	--enable-multibyte \
	--enable-pcre \
	--with-tcsetpgrp
make
make DESTDIR=$PKG install
