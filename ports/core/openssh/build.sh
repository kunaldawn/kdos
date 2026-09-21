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

# THE SECURITY-KEY MIDDLEWARE IS BUILT IN, WHICH IS WHAT MAKES `-sk` KEYS
# WORK. Without it ssh-keygen still knows the ed25519-sk and ecdsa-sk key
# TYPES and refuses every one of them at run time — "no FIDO SecurityKeyProvider
# specified" — which reads as an unsupported key rather than a missing library.
# `builtin` links libfido2 directly instead of dlopening a provider the image
# would then have to ship and name.
./configure --prefix=/usr                     \
			--sysconfdir=/etc/ssh             \
			--libexecdir=/usr/lib/$name       \
			--with-md5-passwords              \
			--with-privsep-path=/var/lib/sshd \
			--without-zlib-version-check     \
			--with-security-key-builtin
make
make DESTDIR=$PKG install

install -v -m755 contrib/ssh-copy-id $PKG/usr/bin
install -v -m644 contrib/ssh-copy-id.1 $PKG/usr/share/man/man1
install -dm700 -o root -g sys $PKG/var/lib/sshd
