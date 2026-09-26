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

# --with-libpcap, --with-libssh2, --with-libz and --with-libpcre take `yes` and
# still fall back to the copies bundled in the tarball when the system library
# is missing, so `depends` is what holds them to the system ones. PCRE2 matters
# most: it runs version detection's regexes over banners from the network. The DIR spelling is no
# stronger and puts -I/usr/include ahead of libstdc++'s own headers, which
# breaks their #include_next.
#
# --with-liblinear=included: configure otherwise links any system linear.h it
# finds, and liblinear is not a port.
#
# --without-ndiff keeps upstream's install-ndiff out: it runs pip with build
# isolation, which fetches setuptools. ndiff is installed below with the
# installed setuptools instead.
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
make
make DESTDIR=$PKG install

pip3 install --no-deps --no-index --no-build-isolation --root=$PKG --prefix=/usr ./ndiff
install -Dm644 ndiff/docs/ndiff.1 -t "$PKG/usr/share/man/man1"

# jdwp-exec and jdwp-info inject the precompiled Java classes under
# nselib/data/jdwp-class into the target; the package carries no prebuilt
# bytecode, so the classes go and the two scripts that load them go with them.
# jdwp-inject reads a class the user names and stays. script.db indexes every
# script by name, so it is regenerated from the staged tree, or `--script
# default` would name jdwp-info and fail to find it.
rm -f "$PKG"/usr/share/nmap/nselib/data/jdwp-class/*.class \
	"$PKG"/usr/share/nmap/scripts/jdwp-exec.nse \
	"$PKG"/usr/share/nmap/scripts/jdwp-info.nse
./nmap --datadir "$PKG/usr/share/nmap" --script-updatedb
