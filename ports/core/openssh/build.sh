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
			--with-md5-passwords              \
			--with-privsep-path=/var/lib/sshd \
			--without-zlib-version-check
make
make DESTDIR=$PKG install

install -v -m755 contrib/ssh-copy-id $PKG/usr/bin
install -v -m644 contrib/ssh-copy-id.1 $PKG/usr/share/man/man1
install -dm700 -o root -g sys $PKG/var/lib/sshd
