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
	--libdir=/usr/lib \
	--sysconfdir=/etc \
	--localstatedir=/var \
	--with-menudir=/usr/share/applications \
	--with-icondir=/usr/share/icons \
	--with-logdir=/var/log/cups \
	--with-docdir=/usr/share/cups \
	--with-rundir=/run/cups \
	--with-cupsd-file-perm=0755 \
	--with-cups-user=lp \
	--with-cups-group=lp \
	--with-system-groups=lpadmin \
	--with-domainsocket=/run/cups/cups.sock \
	--enable-acl \
	--enable-dbus \
	--with-dbusdir=/usr/share/dbus-1 \
	--enable-libusb \
	--enable-raw-printing \
	--enable-relro \
	--with-tls=gnutls \
	--with-optim="$CFLAGS" \
	--without-rcdir \
	--without-systemd
make
make BUILDROOT=$PKG install

# Linux PAM Configuration
install -d -m 755 $PKG/etc/pam.d
cat > $PKG/etc/pam.d/cups << "EOF"
auth    required pam_unix.so
account required pam_unix.so
EOF
chmod 644 $PKG/etc/pam.d/cups

# cleanup
rm -fr $PKG/tmp $PKG/run $PKG/var/run

chmod 0755 $PKG/var/cache
chmod 0755 $PKG/var/spool
chmod -R +w $PKG
