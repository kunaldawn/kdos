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

# The driver manager only: no bundled drivers, since every database's driver
# (sqliteodbc, MariaDB's, PostgreSQL's) is its own port and registers itself in
# /etc/odbcinst.ini. libltdl comes from the libtool port; the bundled copy
# would put a second libltdl.so on the system. install creates the empty
# /etc/odbc.ini and /etc/odbcinst.ini that isql and every client read.
./configure --prefix=/usr --sysconfdir=/etc --libdir=/usr/lib \
	--disable-static \
	--enable-threads \
	--enable-readline \
	--enable-iconv \
	--disable-drivers \
	--disable-driver-config \
	--disable-stats \
	--without-included-ltdl \
	--disable-ltdl-install
make
make DESTDIR=$PKG install
