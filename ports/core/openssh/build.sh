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
#
# --with-libedit gives sftp line editing and history. --with-ldns is what
# lets VerifyHostKeyDNS trust an SSHFP record: without it the answer is not
# DNSSEC-validated and ssh asks about the key anyway. --with-kerberos5 builds
# GSSAPI authentication for hosts in a Kerberos realm; it stays off in the
# shipped ssh_config and sshd_config until someone turns it on.
./configure --prefix=/usr                     \
			--sysconfdir=/etc/ssh             \
			--libexecdir=/usr/lib/$name       \
			--with-privsep-path=/var/lib/sshd \
			--without-zlib-version-check     \
			--with-security-key-builtin       \
			--with-libedit                    \
			--with-ldns                       \
			--with-kerberos5=/usr
make
make DESTDIR=$PKG install

install -v -m755 contrib/ssh-copy-id $PKG/usr/bin
install -v -m644 contrib/ssh-copy-id.1 $PKG/usr/share/man/man1
install -dm700 -o root -g sys $PKG/var/lib/sshd
