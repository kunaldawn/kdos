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
#
# --with-system-tzdata makes time zones follow the tzdata port rather than a
# copy frozen into this build.
#
# --without-llvm: the JIT provider's version guards in 18.6 stop at LLVM 22,
# and the llvm port is 23. --without-ldap: openldap is not a port.
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
	--with-perl \
	--with-pam \
	--with-gssapi \
	--with-libcurl \
	--with-liburing \
	--with-uuid=e2fs \
	--with-system-tzdata=/usr/share/zoneinfo \
	--without-tcl \
	--without-ldap \
	--without-llvm \
	--disable-rpath
make world-bin
make -C doc/src/sgml man
make DESTDIR=$PKG install-world-bin
make -C doc/src/sgml DESTDIR=$PKG install-man
