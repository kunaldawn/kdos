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
	--sysconfdir=/etc/zsh \
	--enable-etcdir=/etc/zsh \
	--enable-multibyte \
	--enable-pcre \
	--enable-libc-musl \
	--enable-maildir-support \
	--disable-gdbm \
	--disable-cap
grep -q '^name=zsh/pcre .*link=dynamic' config.modules || {
	echo "zsh: the pcre module is off — pcre2-config was not found" >&2
	exit 1
}
make
make DESTDIR=$PKG install

install -Dm644 /dev/stdin "$PKG/etc/zsh/zprofile" <<'ZPROFILE'
emulate sh -c '. /etc/profile'
ZPROFILE

install -Dm644 /dev/stdin "$PKG/etc/zsh/zshrc" <<'ZSHRC'
if (( $+commands[atuin] )); then
	eval "$(atuin init zsh --disable-up-arrow)"
fi
ZSHRC
