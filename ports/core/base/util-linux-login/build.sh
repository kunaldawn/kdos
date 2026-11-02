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

patch -p1 -i "$PORT_SRC/utmps-paths.patch"
./configure \
	--prefix=/usr \
	--disable-all-programs \
	--enable-login \
	--enable-login-lastlog \
	--without-systemd \
	--without-systemdsystemunitdir \
	--without-udev \
	--without-python \
	--without-selinux \
	--without-audit \
	--without-econf \
	--disable-asciidoc \
	--disable-poman \
	--disable-nls \
	CPPFLAGS=-I/usr/include/utmps
make LIBS="-lutmps -lskarnet" login
install -D -m 755 login "$PKG/usr/bin/login"
install -D -m 644 login-utils/login.1 "$PKG/usr/share/man/man1/login.1"

install -d -m 755 "$PKG/etc/pam.d"
cat > "$PKG/etc/pam.d/login" << "EOF"
# Begin /etc/pam.d/login
auth      optional   pam_faildelay.so  delay=3000000
auth      requisite  pam_nologin.so
auth      include    system-login
account   include    system-login
password  include    system-login
session   include    system-login
# End /etc/pam.d/login
EOF
chmod 644 "$PKG/etc/pam.d/login"
