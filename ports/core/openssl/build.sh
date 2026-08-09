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

./config \
	--prefix=/usr \
	--libdir=lib \
	--openssldir=/etc/ssl \
	enable-ec_nistp_64_gcc_128 \
	enable-camellia \
	enable-seed \
	enable-rfc3779 \
	enable-ktls \
	enable-argon2 \
	no-mdc2 \
	no-ec2m \
	no-sm2 \
	no-sm4 \
	shared \
	threads \
	zlib

make depend
make
make MANSUFFIX=ssl DESTDIR=$PKG install_sw install_ssldirs install_man_docs
