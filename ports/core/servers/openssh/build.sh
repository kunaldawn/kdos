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

./configure --prefix=/usr                     \
			--sysconfdir=/etc/ssh             \
			--libexecdir=/usr/lib/$name       \
			--with-privsep-path=/var/lib/sshd \
			--without-zlib-version-check     \
			--with-security-key-builtin       \
			--with-libedit                    \
			--with-ldns                       \
			--with-xauth=/usr/bin/xauth       \
			--with-kerberos5=/usr
make
make DESTDIR=$PKG install

install -v -m755 contrib/ssh-copy-id $PKG/usr/bin
install -v -m644 contrib/ssh-copy-id.1 $PKG/usr/share/man/man1
install -dm700 -o root -g sys $PKG/var/lib/sshd

install -Dm644 /dev/stdin "$PKG/etc/profile.d/50-ssh-agent.sh" <<'KDOS_SH'
if [ -z "$SSH_AUTH_SOCK" ] && [ -n "$XDG_RUNTIME_DIR" ] && [ -d "$XDG_RUNTIME_DIR" ]; then
	SSH_AUTH_SOCK="$XDG_RUNTIME_DIR/ssh-agent.socket"
	SSH_AUTH_SOCK="$SSH_AUTH_SOCK" ssh-add -l >/dev/null 2>&1
	if [ $? -eq 2 ]; then
		rm -f "$SSH_AUTH_SOCK"
		ssh-agent -a "$SSH_AUTH_SOCK" >/dev/null
	fi
	export SSH_AUTH_SOCK
fi
KDOS_SH
