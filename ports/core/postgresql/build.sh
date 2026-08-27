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

# --with-icu IS THE ONE FLAG THAT CHANGES ANSWERS. musl's collation is
# essentially byte order, so a database built against it sorts and compares
# text differently from every other machine the data will touch — `ORDER BY`
# on anything but ASCII, `LIKE` and unique indexes on case-folded text all
# quietly disagree. ICU is already a port and is what makes those correct.
#
# sqlite is here and is right for one program's data; this is for the moment
# there are two, or two PEOPLE, and the file-locking answer stops working.
#
# --with-systemd is deliberately absent rather than disabled: it is off by
# default, and there is nothing here to integrate with.
./configure \
	--prefix=/usr \
	--sysconfdir=/etc \
	--libdir=/usr/lib \
	--datarootdir=/usr/share \
	--with-icu \
	--with-openssl \
	--with-readline \
	--with-libxml \
	--with-libxslt \
	--with-lz4 \
	--with-zstd \
	--with-python \
	--without-perl \
	--without-tcl \
	--without-pam \
	--without-ldap \
	--disable-rpath
make world-bin
make DESTDIR=$PKG install-world-bin
