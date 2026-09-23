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

patch -p1 -i "$PORT_SRC/openssl4.patch"

# A CLIENT, NOT A REALM. What this image needs Kerberos for is a ticket a file
# server will accept: `kinit` gets one, `cifs.upcall` hands it to the kernel's
# cifs module, and `sec=krb5` then mounts a share on a machine that will not
# take a password at all. The KDC, kadmind and the database back ends are the
# other half of the package and nothing here runs one, so they are not
# installed — a key distribution centre on a laptop is a listening socket
# nobody asked for and a database nobody backs up.
#
# THE CONFIGURE SCRIPT IS IN `src`, not at the top of the tarball. Everything
# above it is documentation.
cd src

# OPENSSL RATHER THAN THE BUILT-IN CRYPTO. Both are supported upstream; this
# image already carries and updates openssl, and a second AES and a second
# SHA-2 on the same disk are a second set of advisories to track.
#
# --without-system-verto AND THE BUNDLED COPY IS BUILT. libverto is the event
# abstraction the KDC and kadmind use, there is no port for it, and configure
# stops rather than choosing: "Could not find libverto". The bundled one is
# ~1500 lines and is linked into libkrb5support, which the client needs.
#
# --without-readline AND --without-libedit: the interactive shell they give a
# line editor to is kadmin's, and kadmin administers a realm this image does
# not serve.
#
# --disable-rpath because every library lands in /usr/lib, and an rpath naming
# the build tree is one the package manager cannot rewrite.
#
# --disable-nls AND IT IS musl THAT DECIDES. krb5 calls `dgettext` directly
# and its configure looks for the symbol in the C library; glibc has it and
# musl does not, so on this image the probe finds `libintl.h` from the gettext
# port, concludes translation is available and then links without `-lintl` —
# `undefined reference to libintl_dgettext`, from the first shared library it
# builds. What is given up is translated messages from the Kerberos libraries,
# which are the only strings involved.
#
# --enable-dns-for-realm IS WHAT MAKES A REALM FINDABLE. Without it a client
# needs every realm's KDC written out in krb5.conf by hand; with it the SRV
# records a corporate domain already publishes are enough, which is the shape
# `sec=krb5` is used in.
./configure \
	--prefix=/usr \
	--sysconfdir=/etc \
	--localstatedir=/var \
	--libdir=/usr/lib \
	--runstatedir=/run \
	--sbindir=/usr/sbin \
	--disable-static \
	--disable-rpath \
	--disable-nls \
	--enable-dns-for-realm \
	--with-crypto-impl=openssl \
	--with-system-et \
	--without-system-verto \
	--without-ldap \
	--without-readline \
	--without-libedit \
	--without-lmdb
make
make DESTDIR=$PKG install

# THE SERVER HALF, REMOVED AFTER THE INSTALL RATHER THAN BEFORE THE BUILD.
# There is no configure flag that builds a client alone, and a partial `make`
# in a tree with this many inter-library dependencies fails in places that have
# nothing to do with the KDC. So the whole thing is built and what a client
# never runs is taken back out — a key distribution centre, its administration
# daemon, the database utilities and the propagation pair.
for _s in krb5kdc kadmind kadmin.local kdb5_util kprop kpropd \
	  kproplog sserver gss-server sim_server uuserver krb5-send-pr; do
	rm -f "$PKG/usr/sbin/$_s"
done

# AND THE DEMONSTRATION CLIENTS THAT PAIR WITH THEM. `gss-client`, `sclient`,
# `sim_client` and `uuclient` are the tutorial's other half — each talks to one
# of the servers just removed — and `krb5-send-pr` mails a bug report through a
# mail transport this image does not configure.
for _s in gss-client sclient sim_client uuclient krb5-send-pr; do
	rm -f "$PKG/usr/bin/$_s"
done

# `ksu` IS AN EIGHTEENTH setuid-root BINARY AND IT IS NOT ONE THIS IMAGE NEEDS.
# It is `su` for a Kerberos principal, on a machine whose accounts are local
# and whose privilege escalation is `sudo`; what Kerberos is here for is a
# ticket a file server will take. The setuid inventory in the security model is
# an exact count, and an entry nothing uses is one nobody audits.
rm -f "$PKG/usr/bin/ksu"
rm -f "$PKG/usr/share/man/man1/ksu.1"*

# AND THE MANUALS FOR THEM GO WITH THEM: a page for a program that is not on
# the disk is a page that sends somebody looking for it.
for _s in krb5kdc kadmind kadmin.local kdb5_util kdb5_ldap_util kprop kpropd \
	  kproplog sserver sclient kdc.conf kadm5.acl; do
	rm -f "$PKG/usr/share/man/man1/$_s."* \
	      "$PKG/usr/share/man/man5/$_s."* "$PKG/usr/share/man/man8/$_s."*
done

# THE UPCALL NEEDS A RULE, AND IT IS cifs-utils' FILE IN krb5's PACKAGE ONLY
# BY ACCIDENT OF WHO WRITES IT. `request-key` is what the kernel's cifs module
# reaches for when it wants a ticket, and it consults /etc/request-key.d — a
# directory keyutils owns and ships empty. Without the rule the mount fails
# with `Required key not available` and nothing says which key.
#
# ONE RULE AND NOT TWO: `cifs.idmap` is not built here — it needs winbind's
# client library, which samba on this image is not built with — so a rule
# naming it would point `request-key` at a file that is not there.
install -Dm644 /dev/stdin "$PKG/etc/request-key.d/cifs.spnego.conf" <<'RK'
create cifs.spnego * * /usr/sbin/cifs.upcall %k
RK
