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
#
# --enable-pcre ONLY ASKS: a missing pcre2-config turns the zsh/pcre module
# off with a warning. config.modules is checked after configure so that stops
# the build instead. --enable-libc-musl selects the POSIX feature macros musl
# needs; --enable-maildir-support lets MAIL and MAILPATH name a Maildir, which
# is what isync, aerc and notmuch deliver to. gdbm and cap stay off.
./configure \
	--prefix=/usr \
	--sysconfdir=/etc/zsh \
	--enable-etcdir=/etc/zsh \
	--enable-multibyte \
	--enable-pcre \
	--enable-libc-musl \
	--enable-maildir-support \
	--disable-gdbm \
	--disable-cap \
	--with-tcsetpgrp
grep -q '^name=zsh/pcre .*link=dynamic' config.modules || {
	echo "zsh: the pcre module is off — pcre2-config was not found" >&2
	exit 1
}
make
make DESTDIR=$PKG install
