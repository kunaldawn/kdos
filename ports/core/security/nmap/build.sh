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
	--without-zenmap \
	--without-ndiff \
	--with-openssl \
	--with-libpcap=yes \
	--with-libssh2=yes \
	--with-libz=yes \
	--with-libpcre=yes \
	--with-liblinear=included \
	--with-liblua=included
make -C liblinear liblinear.a CC="gcc -std=gnu11" CXX=g++ \
	CFLAGS="${CFLAGS/-std=gnu11/} -Wno-uninitialized"
make
make DESTDIR=$PKG install

pip3 install --ignore-installed --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr ./ndiff
install -Dm644 ndiff/docs/ndiff.1 -t "$PKG/usr/share/man/man1"

rm -f "$PKG"/usr/share/nmap/nselib/data/jdwp-class/*.class \
	"$PKG"/usr/share/nmap/scripts/jdwp-exec.nse \
	"$PKG"/usr/share/nmap/scripts/jdwp-info.nse
./nmap --datadir "$PKG/usr/share/nmap" --script-updatedb
