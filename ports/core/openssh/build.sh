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
#
# configure links libmd for SHA256Update whenever it is installed, and has no
# switch to refuse it, so libmd is in `depends`: left out, whether sshd needs
# libmd.so.0 would follow build order.
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

# ONE AGENT PER ACCOUNT, AT A FIXED PATH. A login shell reads profile.d and the
# desktop inherits its environment, so exporting SSH_AUTH_SOCK here reaches
# every terminal and launcher of the session, and a key added once is used by
# all of them. The socket lives in the runtime directory 10-wayland.sh has
# already made, and a later login — tty2, ssh — finds the agent answering and
# reuses it rather than starting a second. `ssh-add -l` exits 2 only when
# nothing answers; 0 and 1 are an agent with keys and one without. A value
# somebody exported first (ssh -A, gpg-agent's socket) is left alone.
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
