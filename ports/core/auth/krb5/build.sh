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

cd src

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
	--with-tls-impl=openssl \
	--with-spake-openssl \
	--enable-pkinit \
	--with-keyutils \
	--with-system-et \
	--without-system-verto \
	--without-ldap \
	--without-readline \
	--without-libedit \
	--without-lmdb
make
make DESTDIR=$PKG install
rm -rf "$PKG/run"

for _s in krb5kdc kadmind kadmin.local kdb5_util kprop kpropd \
	  kproplog sserver gss-server sim_server uuserver krb5-send-pr; do
	rm -f "$PKG/usr/sbin/$_s"
done

for _s in gss-client sclient sim_client uuclient krb5-send-pr; do
	rm -f "$PKG/usr/bin/$_s"
done

rm -f "$PKG/usr/bin/ksu"
rm -f "$PKG/usr/share/man/man1/ksu.1"*

for _s in krb5kdc kadmind kadmin.local kdb5_util kdb5_ldap_util kprop kpropd \
	  kproplog sserver sclient kdc.conf kadm5.acl; do
	rm -f "$PKG/usr/share/man/man1/$_s."* \
	      "$PKG/usr/share/man/man5/$_s."* "$PKG/usr/share/man/man8/$_s."*
done

install -Dm644 /dev/stdin "$PKG/etc/request-key.d/cifs.spnego.conf" <<'RK'
create cifs.spnego * * /usr/sbin/cifs.upcall %k
RK
