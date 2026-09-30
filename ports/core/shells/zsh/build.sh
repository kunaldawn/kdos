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

# /etc/zsh/zprofile, which a login zsh reads first. The environment every
# session depends on — PATH, the XDG directories, the runtime directory and
# everything in /etc/profile.d — lives in /etc/profile, which zsh never reads
# on its own; without this an account whose login shell is zsh has none of it.
# `emulate sh` because /etc/profile and its drop-ins are POSIX sh.
install -Dm644 /dev/stdin "$PKG/etc/zsh/zprofile" <<'ZPROFILE'
emulate sh -c '. /etc/profile'
ZPROFILE

# /etc/zsh/zshrc, the file --enable-etcdir makes every interactive zsh read.
# It carries the hooks of the image's shell tools that have a zsh side, each
# probed before it is used. atuin's zsh init records through zsh's own
# preexec and precmd hooks, which bash needs bash-preexec for.
install -Dm644 /dev/stdin "$PKG/etc/zsh/zshrc" <<'ZSHRC'
if (( $+commands[atuin] )); then
	eval "$(atuin init zsh --disable-up-arrow)"
fi
ZSHRC
